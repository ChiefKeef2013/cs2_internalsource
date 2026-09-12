#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <windows.h>

namespace economy {

	struct http_response
	{
		int status{};
		std::string body;
		bool ok( ) const { return status >= 200 && status < 300; }
	};

	// External networking removed: the cheat must not talk to any server outside unknowncheats.me.
	// These helpers previously performed WinHTTP GET/POST against a backend (login, heartbeat,
	// match reporting, cloud configs, forum avatar scraping). They are intentionally inert now.
	inline http_response http_post( const std::wstring& /*host*/, const std::wstring& /*path*/,
		const std::string& /*json_body*/, const std::vector<std::wstring>& /*extra_headers*/ )
	{
		return {};
	}

	inline http_response http_get( const std::string& /*url*/, std::string* final_url = nullptr, std::size_t max_bytes = 0 )
	{
		if ( final_url )
			final_url->clear( );
		( void ) max_bytes;
		return {};
	}

	inline std::string resolve_forum_avatar( const std::string& username )
	{
		( void ) username;
		return {};
	}

	namespace detail {

		inline std::string json_error( const http_response& resp )
		{
			return "HTTP " + std::to_string( resp.status );
		}

	}

	struct login_result
	{
		bool success{};
		std::string session;
		std::string display_name;
		std::string expiration;
		std::string error_message;
	};

	struct match_start_result
	{
		bool success{};
		std::string match_token;
		std::string error_message;
	};

	struct match_report_result
	{
		bool accepted{};
		bool clamped{};
		int credited_gold{};
		int credited_silver{};
		int credited_bronze{};
		bool wallet_linked{};
		std::string error_message;
	};

	struct profile_result
	{
		bool success{};
		bool linked{};
		std::string username;
		std::string avatar_url;
		int wallet_gold{};
		int wallet_silver{};
		int wallet_bronze{};
		std::string badge_name;
		std::string badge_image_url;
		std::string name_style_name;
		std::string name_style_effect_key;
		std::string name_style_rarity;
		std::string error_message;
	};

	struct config_entry
	{
		std::string name;
		std::string share_code;
		std::string created_at;
		std::string updated_at;
		int load_count{};
		bool owned{};
		std::string author;
		bool can_rename{ true };
	};

	struct config_list_result
	{
		bool success{};
		std::vector<config_entry> configs;
		std::string error_message;
	};

	struct config_save_result
	{
		bool success{};
		bool updated{};
		std::string share_code;
		std::string name;
		std::string error_message;
	};

	struct config_load_result
	{
		bool success{};
		std::string name;
		std::string config_json;
		std::string share_code;
		std::string error_message;
	};

	struct config_delete_result
	{
		bool success{};
		std::string error_message;
	};

	// Offline stub: all server communication removed.
	class api_client
	{
	public:
		login_result login( const std::string& key, const std::string& hwid, const std::string& username )
		{
			( void ) key; ( void ) hwid; ( void ) username;
			login_result out;
			out.error_message = "Disabled.";
			return out;
		}

		http_response heartbeat( const std::string& session, const std::string& key, const std::string& hwid )
		{
			( void ) session; ( void ) key; ( void ) hwid;
			return {};
		}

		match_start_result match_start( const std::string& session, const std::string& key,
			const std::string& hwid, const std::string& map, const std::string& mode )
		{
			( void ) session; ( void ) key; ( void ) hwid; ( void ) map; ( void ) mode;
			return {};
		}

		match_report_result match_report( const std::string& session, const std::string& key,
			const std::string& hwid, const std::string& match_token,
			int headshot_kills, int body_kills, int assists, int duration_seconds )
		{
			( void ) session; ( void ) key; ( void ) hwid; ( void ) match_token;
			( void ) headshot_kills; ( void ) body_kills; ( void ) assists; ( void ) duration_seconds;
			return {};
		}

		profile_result profile( const std::string& session, const std::string& key, const std::string& hwid )
		{
			( void ) session; ( void ) key; ( void ) hwid;
			return {};
		}

		config_list_result config_list( const std::string& session, const std::string& key, const std::string& hwid )
		{
			( void ) session; ( void ) key; ( void ) hwid;
			return {};
		}

		config_save_result config_save( const std::string& session, const std::string& key, const std::string& hwid,
			const std::string& name, const std::string& config_json, const std::string& share_code = {} )
		{
			( void ) session; ( void ) key; ( void ) hwid; ( void ) name; ( void ) config_json; ( void ) share_code;
			config_save_result out;
			out.error_message = "Disabled.";
			return out;
		}

		config_load_result config_load( const std::string& share_code )
		{
			( void ) share_code;
			config_load_result out;
			out.error_message = "Disabled.";
			return out;
		}

		config_delete_result config_delete( const std::string& session, const std::string& key,
			const std::string& hwid, const std::string& share_code )
		{
			( void ) session; ( void ) key; ( void ) hwid; ( void ) share_code;
			config_delete_result out;
			out.error_message = "Disabled.";
			return out;
		}
	};

}
