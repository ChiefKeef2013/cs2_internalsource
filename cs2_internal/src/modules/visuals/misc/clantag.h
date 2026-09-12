#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {

	namespace {

		constexpr const char* k_clantag_text{ "cs2_internal" };
		constexpr const char* k_name_key{ "name" };
		constexpr std::int64_t k_update_interval_ms{ 400 };

		[[nodiscard]] c_convar* name_cvar( )
		{
			return addresses::globals::cvar
				? addresses::globals::cvar->find( "name"_hash )
				: nullptr;
		}

		[[nodiscard]] const char* current_name( )
		{
			const auto name = name_cvar( );
			return name ? name->m_value.sz : nullptr;
		}

		[[nodiscard]] bool looks_generated( const std::string& value, const std::string& tag )
		{
			if ( value.empty( ) || tag.empty( ) )
			{
				return false;
			}

			for ( std::size_t count = 1; count <= tag.length( ); ++count )
			{
				const auto prefix = tag.substr( 0, count ) + " ";
				if ( value.rfind( prefix, 0 ) == 0 )
				{
					return true;
				}
			}

			return false;
		}

		void prepare_name_cvar( )
		{
			const auto name = name_cvar( );
			if ( !name )
			{
				return;
			}

			constexpr std::uint64_t fcvar_protected = 1ull << 5;
			constexpr std::uint64_t fcvar_userinfo = 1ull << 9;
			constexpr std::uint64_t fcvar_registry_restricted = 1ull << 10;

			const auto flags_address = reinterpret_cast< std::uintptr_t >( name ) + offsetof( c_convar, m_flags );
			const auto flags = memory::read<std::uint64_t>( flags_address );
			memory::write<std::uint64_t>( flags_address,
				( flags | fcvar_userinfo ) & ~( fcvar_protected | fcvar_registry_restricted ) );
		}

		void submit_name( const std::string& display_name )
		{
			if ( display_name.empty( ) )
			{
				return;
			}

			clantag::s_display_name = display_name;
			clantag::s_pending = true;
			memory::call<void>( PATTERN( PATTERN_ENGINE_CLIENT_CMD ), addresses::globals::source2engine_to_client, 0, xs( "setinfo name x" ), 0x7ffef001 );
			clantag::s_pending = false;
		}

	}

	void clantag::remember_original( )
	{
		const auto& name_changer_cfg = settings::g_misc.m_name_changer;
		if ( name_changer_cfg.override_name.value && !name_changer_cfg.name.value.empty( ) )
		{
			return;
		}

		const auto current = current_name( );
		if ( !current || !current[ 0 ] )
		{
			return;
		}

		const auto value = std::string{ current };
		if ( looks_generated( value, k_clantag_text ) )
		{
			return;
		}

		this->m_original_name = value;
		this->m_has_original = true;
	}

	std::string clantag::animated_name( )
	{
		if ( !this->m_has_original )
		{
			this->remember_original( );
		}

		const std::string base{ k_clantag_text };

		const auto now = std::chrono::steady_clock::now( );
		const auto ms = std::chrono::duration_cast< std::chrono::milliseconds >( now.time_since_epoch( ) ).count( );

		const auto length = static_cast< int >( base.length( ) );
		const auto total_steps = length * 2;
		const auto step = total_steps > 0
			? static_cast< int >( ( ms / k_update_interval_ms ) % total_steps )
			: 0;

		auto animated = step < length
			? base.substr( 0, static_cast< std::size_t >( step + 1 ) )
			: base.substr( 0, static_cast< std::size_t >( total_steps - step ) );

		if ( animated.empty( ) )
		{
			animated = " ";
		}

		const auto& name_changer_cfg = settings::g_misc.m_name_changer;
		const auto base_name = ( name_changer_cfg.override_name.value && !name_changer_cfg.name.value.empty( ) )
			? name_changer_cfg.name.value
			: ( this->m_has_original ? this->m_original_name : std::string{ "player" } );

		return animated + " " + base_name;
	}

	void clantag::on_frame_stage_notify( )
	{
		const auto& cfg = settings::g_misc.m_clantag;
		const auto local = systems::g_local.get( );
		const auto in_game = local.is_valid( );

		if ( !in_game )
		{
			this->m_was_enabled = false;
			this->m_last_update = {};
			return;
		}

		const auto now = std::chrono::steady_clock::now( );
		const auto due = this->m_last_update.time_since_epoch( ).count( ) == 0
			|| std::chrono::duration_cast< std::chrono::milliseconds >( now - this->m_last_update ).count( ) >= k_update_interval_ms;

		if ( cfg.enabled.value )
		{
			this->remember_original( );
			if ( due )
			{
				submit_name( this->animated_name( ) );
				this->m_last_update = now;
			}

			this->m_was_enabled = true;
			return;
		}

		if ( this->m_was_enabled && this->m_has_original )
		{
			submit_name( this->m_original_name );
		}

		this->m_was_enabled = false;
		this->m_last_update = {};
	}

	bool clantag::on_set_info( std::uintptr_t command )
	{
		const auto& cfg = settings::g_misc.m_clantag;
		if ( ( !cfg.enabled.value && !clantag::s_pending ) || !command )
		{
			return false;
		}

		const auto arg_list = memory::read<std::uintptr_t>( command + 0x440 );
		const auto key = arg_list
			? memory::read<const char*>( arg_list + 0x8 )
			: nullptr;

		if ( !key || _stricmp( key, k_name_key ) != 0 )
		{
			return false;
		}

		this->remember_original( );
		prepare_name_cvar( );

		if ( cfg.enabled.value )
		{
			clantag::s_display_name = this->animated_name( );
		}

		if ( clantag::s_display_name.empty( ) )
		{
			return false;
		}

		static char name_buf[ 128 ]{};
		std::memset( name_buf, 0, sizeof( name_buf ) );
		std::snprintf( name_buf, sizeof( name_buf ), "%s", clantag::s_display_name.c_str( ) );
		memory::write<const char*>( arg_list + 0x10, name_buf );
		return true;
	}

}

