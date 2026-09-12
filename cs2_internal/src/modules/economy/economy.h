#pragma once

#include <modules/economy/api_client.h>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <chrono>
#include <atomic>

namespace economy {

	class match_tracker
	{
	public:
		void set_credentials( const std::string& key, const std::string& hwid, const std::string& username )
		{
			std::unique_lock lock( m_mtx );
			m_key = key;
			m_hwid = hwid;
			m_username = username;
			m_has_credentials.store( !key.empty( ) && !hwid.empty( ), std::memory_order_release );

			if ( m_display_name.empty( ) && !username.empty( ) )
			{
				m_display_name = username;
			}
		}

		bool has_credentials( ) const
		{
			return m_has_credentials.load( std::memory_order_acquire );
		}

		void stash_session( const std::string& session )
		{
			if ( session.empty( ) )
			{
				return;
			}
			std::unique_lock lock( m_mtx );
			m_session = session;
			m_logged_in.store( true );
		}

		void invalidate_session( )
		{
			std::unique_lock lock( m_mtx );
			m_session.clear( );
			m_logged_in.store( false );
			m_match_active = false;
			m_match_token.clear( );
		}

		void async_login( )
		{
			if ( !has_credentials( ) || m_logged_in.load( ) )
			{
				return;
			}

			std::thread( [ this ]
			{
				std::string key, hwid, user;
				{
					std::shared_lock lock( m_mtx );
					key = m_key;
					hwid = m_hwid;
					user = m_username;
				}

				api_client client;
				const auto result = client.login( key, hwid, user );

				std::unique_lock lock( m_mtx );
				if ( result.success )
				{
					m_session = result.session;
					if ( !result.display_name.empty( ) )
					{
						m_display_name = result.display_name;
					}
					else if ( m_display_name.empty( ) && !user.empty( ) )
					{
						m_display_name = user;
					}
					m_logged_in.store( true );
				}
				else
				{
					m_logged_in.store( false );
				}
			} ).detach( );
		}

		void async_heartbeat( )
		{
			if ( !m_logged_in.load( ) )
			{
				return;
			}

			const auto now = std::chrono::steady_clock::now( );
			if ( now - m_last_heartbeat < std::chrono::seconds( 90 ) )
			{
				return;
			}

			m_last_heartbeat = now;

			std::thread( [ this ]
			{
				std::string session, key, hwid;
				{
					std::shared_lock lock( m_mtx );
					session = m_session;
					key = m_key;
					hwid = m_hwid;
				}

				api_client client;
				const auto resp = client.heartbeat( session, key, hwid );
				if ( !resp.ok( ) )
				{
					if ( resp.status == 401 || resp.status == 403 || resp.status == 440 )
						invalidate_session( );
				}
			} ).detach( );
		}

		void on_match_start( const std::string& map_name = {} )
		{
			if ( !m_logged_in.load( ) )
			{
				return;
			}

			{
				std::unique_lock lock( m_mtx );
				if ( m_match_active )
				{
					return;
				}

				m_headshot_kills = 0;
				m_body_kills = 0;
				m_assists = 0;
				m_match_active = true;
				m_report_pending = false;
				m_match_token.clear( );
				m_match_start_time = std::chrono::steady_clock::now( );
			}

			auto map = map_name.empty( ) ? std::string( "unknown" ) : map_name;

			std::thread( [ this, map ]
			{
				std::string session, key, hwid;
				{
					std::shared_lock lock( m_mtx );
					session = m_session;
					key = m_key;
					hwid = m_hwid;
				}

				api_client client;
				const auto result = client.match_start( session, key, hwid, map, "competitive" );

				bool send_now = false;
				int hs = 0, bd = 0, ast = 0, dur = 0;
				std::string token;
				{
					std::unique_lock lock( m_mtx );
					if ( result.success )
					{
						m_match_token = result.match_token;

						if ( m_report_pending )
						{
							token = m_match_token;
							hs = m_headshot_kills;
							bd = m_body_kills;
							ast = m_assists;
							dur = static_cast<int>( std::chrono::duration_cast<std::chrono::seconds>(
								std::chrono::steady_clock::now( ) - m_match_start_time ).count( ) );
							m_report_pending = false;
							m_match_token.clear( );
							send_now = true;
						}
					}
				}

				if ( send_now )
					fire_report( session, key, hwid, token, hs, bd, ast, dur );
			} ).detach( );
		}

		void on_player_death( std::uintptr_t attacker, std::uintptr_t victim, std::uintptr_t assister,
			std::uintptr_t local_controller, bool headshot, bool same_team )
		{
			if ( !m_logged_in.load( ) )
			{
				return;
			}

			if ( !m_match_active )
			{
				on_match_start( );
			}

			if ( victim == local_controller || same_team )
			{
				return;
			}

			if ( attacker == local_controller )
			{
				std::unique_lock lock( m_mtx );
				if ( headshot )
				{
					++m_headshot_kills;
				}
				else
				{
					++m_body_kills;
				}
				return;
			}

			if ( assister == local_controller )
			{
				std::unique_lock lock( m_mtx );
				++m_assists;
			}
		}

		void on_match_end( )
		{
			if ( !m_logged_in.load( ) )
			{
				return;
			}

			std::string session, key, hwid, token;
			int hs = 0, bd = 0, ast = 0, dur = 0;
			bool send = false;

			{
				std::unique_lock lock( m_mtx );
				if ( !m_match_active && !m_report_pending )
				{
					return;
				}

				m_match_active = false;
				session = m_session;
				key = m_key;
				hwid = m_hwid;
				hs = m_headshot_kills;
				bd = m_body_kills;
				ast = m_assists;
				dur = static_cast<int>( std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::steady_clock::now( ) - m_match_start_time ).count( ) );

				if ( m_match_token.empty( ) )
				{
					m_report_pending = true;
					return;
				}

				token = m_match_token;
				m_match_token.clear( );
				m_report_pending = false;
				send = true;
			}

			if ( send )
				fire_report( session, key, hwid, token, hs, bd, ast, dur );
		}

		void async_fetch_profile( bool force = false )
		{
			if ( !m_logged_in.load( ) )
			{
				return;
			}

			const auto now = std::chrono::steady_clock::now( );
			if ( !force && now - m_last_profile < std::chrono::seconds( 45 ) )
			{
				return;
			}

			if ( m_profile_busy.exchange( true ) )
			{
				return;
			}

			m_last_profile = now;

			std::thread( [ this ]
			{
				std::string session, key, hwid;
				{
					std::shared_lock lock( m_mtx );
					session = m_session;
					key = m_key;
					hwid = m_hwid;
				}

				api_client client;
				const auto result = client.profile( session, key, hwid );

				{
					std::unique_lock lock( m_mtx );
					m_profile = result;
					if ( result.success && result.linked && !result.username.empty( ) )
					{
						m_display_name = result.username;
					}
				}

				m_profile_busy.store( false );
			} ).detach( );
		}

		bool is_logged_in( ) const { return m_logged_in.load( ); }
		bool is_match_active( ) const { std::shared_lock lock( m_mtx ); return m_match_active; }

		profile_result get_profile( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_profile;
		}

		std::string get_display_name( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_display_name.empty( ) ? m_username : m_display_name;
		}

		std::string get_session( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_session;
		}

		std::string get_key( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_key;
		}

		std::string get_hwid( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_hwid;
		}

		struct match_stats
		{
			int headshot_kills{};
			int body_kills{};
			int assists{};
		};

		match_stats get_current_stats( ) const
		{
			std::shared_lock lock( m_mtx );
			return { m_headshot_kills, m_body_kills, m_assists };
		}

	private:
		void fire_report( const std::string& session, const std::string& key, const std::string& hwid,
			const std::string& token, int hs, int bd, int ast, int dur )
		{
			if ( token.empty( ) || ( hs == 0 && bd == 0 && ast == 0 ) )
				return;

			std::thread( [ this, session, key, hwid, token, hs, bd, ast, dur ]
			{
				api_client client;
				const auto result = client.match_report( session, key, hwid, token, hs, bd, ast, dur );
				if ( result.accepted )
					async_fetch_profile( true );
			} ).detach( );
		}

		mutable std::shared_mutex m_mtx;

		std::string m_key;
		std::string m_hwid;
		std::string m_username;
		std::string m_session;
		std::string m_display_name;
		std::string m_match_token;

		std::atomic<bool> m_logged_in{ false };
		std::atomic<bool> m_has_credentials{ false };
		bool m_match_active{};
		bool m_report_pending{};
		int m_headshot_kills{};
		int m_body_kills{};
		int m_assists{};

		std::chrono::steady_clock::time_point m_match_start_time{};
		std::chrono::steady_clock::time_point m_last_heartbeat{};
		std::chrono::steady_clock::time_point m_last_profile{};
		std::atomic<bool> m_profile_busy{ false };

		profile_result m_profile{};
	};

	inline match_tracker g_tracker;

}
