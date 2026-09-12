#pragma once

#include <modules/economy/economy.h>
#include <core/config.hpp>
#include <core/settings.hpp>
#include <atomic>
#include <shared_mutex>
#include <thread>

namespace economy {

	// Cloud config sharing removed: it uploaded/downloaded the user's cheat config
	// to an external server. Only the local config system remains.
	class cloud_configs
	{
	public:
		void refresh( )
		{
			if ( m_busy.exchange( true ) )
				return;

			std::unique_lock lock( m_mtx );
			m_list.success = false;
			m_list.error_message = "Disabled.";
			m_status = "Cloud configs are disabled.";
			m_busy.store( false );
		}

		void save( const std::string& name, const std::string& share_code = {} )
		{
			if ( name.empty( ) || m_busy.exchange( true ) )
				return;

			{
				std::unique_lock lock( m_mtx );
				m_status = "Cloud configs are disabled.";
			}
			m_busy.store( false );
			( void ) share_code;
		}

		void load( const std::string& share_code )
		{
			if ( share_code.empty( ) || m_busy.exchange( true ) )
				return;

			std::unique_lock lock( m_mtx );
			m_status = "Cloud configs are disabled.";
			m_busy.store( false );
		}

		void remove( const std::string& share_code )
		{
			if ( share_code.empty( ) || m_busy.exchange( true ) )
				return;

			std::unique_lock lock( m_mtx );
			m_status = "Cloud configs are disabled.";
			m_busy.store( false );
		}

		bool consume_pending_load( )
		{
			return false;
		}

		bool busy( ) const { return m_busy.load( ); }

		config_list_result list( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_list;
		}

		std::string status( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_status;
		}

		std::string last_share_code( ) const
		{
			std::shared_lock lock( m_mtx );
			return m_last_share_code;
		}

	private:
		mutable std::shared_mutex m_mtx;
		std::atomic<bool> m_busy{ false };
		config_list_result m_list{};
		std::string m_status{ "Disabled." };
		std::string m_last_share_code;
	};

	inline cloud_configs g_cloud;

}
