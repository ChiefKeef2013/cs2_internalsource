#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {

	namespace {

		constexpr std::string_view k_fixed_clantag{ "cs2_internal" };

		[[nodiscard]] std::string controller_name( std::uintptr_t controller )
		{
			if ( !controller )
			{
				return {};
			}

			const auto name_addr = controller + SCHEMA_OFFSET( "CCSPlayerController", "m_sSanitizedPlayerName"_hash );
			const auto name_ptr = memory::read<std::uintptr_t>( name_addr );
			return memory::read_string( name_ptr, 127 );
		}

		[[nodiscard]] std::string strip_clantag_prefix( std::string name )
		{
			while ( name.size( ) >= 3 && name.front( ) == '[' )
			{
				const auto close = name.find( ']' );
				if ( close == std::string::npos || close + 1 >= name.size( ) )
				{
					break;
				}

				auto next = close + 1;
				while ( next < name.size( ) && name[ next ] == ' ' )
				{
					++next;
				}

				name.erase( 0, next );
			}

			return name;
		}

		void submit_name_change( const std::string& display_name )
		{
			if ( display_name.empty( ) )
			{
				return;
			}

			name_changer::s_display_name = display_name;
			name_changer::s_name_change_pending = true;
			memory::call<void>( PATTERN( PATTERN_ENGINE_CLIENT_CMD ), addresses::globals::source2engine_to_client, 0, xs( "setinfo name x" ), 0x7ffef001 );
			name_changer::s_name_change_pending = false;
		}

	}

	void name_changer::on_frame_stage_notify( )
	{
		const auto local = systems::g_local.get( );
		const auto& cfg = settings::g_misc.m_name_changer;
		const auto enabled = cfg.override_name.value;

		if ( !enabled )
		{
			if ( this->m_name_changer_active && local.controller && !this->m_original_name.empty( ) )
			{
				submit_name_change( this->m_original_name );
			}

			this->m_name_changer_active = false;
			this->m_name_changer_controller = 0;
			this->m_original_name.clear( );
			this->m_last_sent_name.clear( );
			name_changer::s_display_name.clear( );
			return;
		}

		if ( !local.controller )
		{
			return;
		}

		if ( !this->m_name_changer_active )
		{
			this->m_original_name = strip_clantag_prefix( controller_name( local.controller ) );
			if ( this->m_original_name.empty( ) )
			{
				this->m_original_name = xs( "Player" );
			}

			this->m_name_changer_active = true;
			this->m_name_changer_controller = local.controller;
			this->m_last_sent_name.clear( );
		}
		else if ( this->m_name_changer_controller != local.controller )
		{
			this->m_name_changer_controller = local.controller;
			this->m_last_sent_name.clear( );
		}

		const auto& configured_name = cfg.name.value;
		const auto& base_name = cfg.override_name.value && !configured_name.empty( )
			? configured_name
			: this->m_original_name;

		std::string display_name = base_name;
		if ( cfg.clantag.value )
		{
			const auto tag = k_fixed_clantag;
			if ( !tag.empty( ) )
			{
				const auto speed = std::max( 0.1f, cfg.clantag_speed.value );
				const auto ticks_per_step = std::max( 1, static_cast< int >( 16.0f / speed ) );

				const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );
				const auto current_tick = global_vars
					? memory::read<int>( global_vars + 0x44 )
					: 0;

				std::string visible_tag{};
				using anim = settings::misc::name_changer::clantag_anim;

				switch ( cfg.clantag_animation.value )
				{
				case anim::static_:
					visible_tag = tag;
					break;

				case anim::scroll:
				{
					const auto len = static_cast< int >( tag.size( ) );
					const auto phase = ( ( current_tick / ticks_per_step ) % len + len ) % len;
					visible_tag = std::string( tag.substr( static_cast< std::size_t >( phase ) ) ) + std::string( tag.substr( 0, static_cast< std::size_t >( phase ) ) );
					break;
				}

				case anim::reverse:
				{
					const auto len = static_cast< int >( tag.size( ) );
					const auto phase = ( ( current_tick / ticks_per_step ) % len + len ) % len;
					const auto idx = ( len - phase ) % len;
					visible_tag = std::string( tag.substr( static_cast< std::size_t >( idx ) ) ) + std::string( tag.substr( 0, static_cast< std::size_t >( idx ) ) );
					break;
				}

				case anim::wave:
				default:
				{
					const auto phase_count = static_cast< int >( tag.size( ) * 2 );
					auto phase = current_tick / ticks_per_step % phase_count;
					if ( phase < 0 )
					{
						phase += phase_count;
					}

					const auto reveal_index = phase <= static_cast< int >( tag.size( ) )
						? phase
						: phase_count - phase;
					visible_tag = std::string( tag.substr( 0, static_cast< std::size_t >( reveal_index ) ) );
					break;
				}
				}

				if ( !visible_tag.empty( ) )
				{
					display_name.reserve( base_name.size( ) + visible_tag.size( ) + 3 );
					display_name = "[";
					display_name += visible_tag;
					display_name += "] ";
					display_name += base_name;
				}
			}
		}

		if ( display_name == this->m_last_sent_name )
		{
			return;
		}

		submit_name_change( display_name );
		this->m_last_sent_name = std::move( display_name );
	}

}

