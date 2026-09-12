#pragma once

#include <core/common.hpp>
#include <core/features.hpp>
#include <core/menu/rendering.hpp>
#include <modules/economy/economy.h>

namespace systems {

	bool events::initialize( )
	{
		if ( !register_listener( xs( "bullet_impact" ), [ ]( void* event ) { features::misc::g_impacts.on_bullet_impact( reinterpret_cast< std::uintptr_t >( event ) ); } ) )
		{
			return false;
		}

		if ( !register_listener( xs( "player_hurt" ), [ ]( void* event )
			{
				const auto ptr = reinterpret_cast< std::uintptr_t >( event );
				features::misc::g_impacts.on_player_hurt( ptr );
				features::misc::g_onshot.on_player_hurt( ptr );

				if ( settings::g_esp.m_player.m_chams.onshot.enabled.value && ptr )
				{
					const auto attacker_key = cstypes::event_hash{ 0, "attacker" };
					const auto userid_key = cstypes::event_hash{ 0, "userid" };
					const auto attacker = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), ptr, &attacker_key );
					const auto local = systems::g_local.get( );
					if ( attacker && attacker == local.controller && local.is_alive )
					{
						const auto victim_pawn = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_PAWN ), ptr, &userid_key );
						if ( victim_pawn && victim_pawn != local.pawn )
						{
							features::esp::player::g_chams.os( ).push( victim_pawn );
						}
					}
				}
			} ) )
		{
			return false;
		}

		if ( !register_listener( xs( "player_death" ), [ ]( void* event )
			{
				const auto ptr = reinterpret_cast< std::uintptr_t >( event );
				features::misc::g_onshot.on_player_death( ptr );

				if ( ptr )
				{
					const auto attacker_key = cstypes::event_hash{ 0, "attacker" };
					const auto userid_key = cstypes::event_hash{ 0, "userid" };
					const auto attacker = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), ptr, &attacker_key );
					const auto local = systems::g_local.get( );
					if ( attacker && attacker == local.controller && local.is_alive )
					{
						const auto victim_pawn = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_PAWN ), ptr, &userid_key );
						if ( victim_pawn && victim_pawn != local.pawn )
						{
							features::esp::player::g_chams.os( ).push( victim_pawn );
						}
					}
				}

				if ( ptr && economy::g_tracker.is_logged_in( ) )
				{
					const auto attacker_key = cstypes::event_hash{ 0, "attacker" };
					const auto userid_key = cstypes::event_hash{ 0, "userid" };
					const auto assister_key = cstypes::event_hash{ 0, "assister" };

					const auto local = systems::g_local.get( );
					if ( local.is_valid( ) && local.controller )
					{
						const auto attacker = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), ptr, &attacker_key );
						const auto victim = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), ptr, &userid_key );
						const auto assister = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), ptr, &assister_key );
						const auto headshot = memory::call<int>( PATTERN( PATTERN_GAME_EVENT_GET_INT ), ptr, "headshot", false ) != 0;

						const auto victim_pawn = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_PAWN ), ptr, &userid_key );
						auto same_team = false;
						if ( victim_pawn )
						{
							const auto victim_team = reinterpret_cast<C_BaseEntity*>( victim_pawn )->m_iTeamNum( );
							same_team = !local.is_this_other_team( victim_team );
						}

						economy::g_tracker.on_player_death( attacker, victim, assister, local.controller, headshot, same_team );
					}
				}
			} ) )
		{
			return false;
		}

		if ( !register_listener( xs( "round_start" ), [ ]( void* event )
			{
				( void )event;
				features::misc::g_autobuy.on_round_start( );
				features::world::g_scene.on_round_start( );

				if ( !economy::g_tracker.is_match_active( ) && economy::g_tracker.is_logged_in( ) )
				{
					economy::g_tracker.on_match_start( rendering::g_widgets.s_map_name );
				}
			} ) )
		{
			return false;
		}

		if ( !register_listener( xs( "cs_win_panel_match" ), [ ]( void* event )
			{
				( void )event;
				economy::g_tracker.on_match_end( );
			} ) )
		{
			return false;
		}

		return true;
	}

	void events::shutdown( )
	{
		for ( auto& entry : m_listeners )
		{
			if ( entry->registered )
			{
				memory::call_vfunc<void>( addresses::globals::game_event_manager, 5, &entry->listener );
				entry->registered = false;
			}
		}

		m_listeners.clear( );
	}

	bool events::register_listener( const char* event_name, handler_fn handler )
	{
		if ( !event_name || !handler )
		{
			return false;
		}

		auto current_entry = std::make_unique<entry>( );
		current_entry->handler = handler;
		current_entry->name = event_name;
		current_entry->registered = false;

		current_entry->vtable_data[ 0 ] = nullptr;
		current_entry->vtable_data[ 1 ] = reinterpret_cast< void* >( &fire_event );
		current_entry->vtable_data[ 2 ] = reinterpret_cast< void* >( &get_debug_id );

		current_entry->listener.vtable = current_entry->vtable_data;
		current_entry->listener.debug_id = static_cast< int >( m_listeners.size( ) + 1 );

		const auto success = memory::call_vfunc<bool>( addresses::globals::game_event_manager, 3, &current_entry->listener, event_name, false );
		if ( !success )
		{
			return false;
		}

		current_entry->registered = true;
		m_listeners.push_back( std::move( current_entry ) );

		return true;
	}

	void events::unregister_listener( const char* event_name )
	{
		if ( !event_name )
		{
			return;
		}

		for ( auto it = m_listeners.begin( ); it != m_listeners.end( ); ++it )
		{
		if ( std::strcmp( ( *it )->name, event_name ) == 0 && ( *it )->registered )
			{
				memory::call_vfunc<void>( addresses::globals::game_event_manager, 5, &( *it )->listener );
				( *it )->registered = false;
				m_listeners.erase( it );
				return;
			}
		}
	}

	void* __fastcall events::fire_event( void* self, void* event )
	{
		if ( systems::g_lifecycle.busy( ) )
		{
			return nullptr;
		}

		const auto current_listener = reinterpret_cast< listener* >( self );

		for ( const auto& entry : m_listeners )
		{
			if ( entry->listener.debug_id == current_listener->debug_id && entry->handler )
			{
				entry->handler( event );
				break;
			}
		}

		return nullptr;
	}

	int __fastcall events::get_debug_id( void* self )
	{
		const auto current_listener = reinterpret_cast< listener* >( self );
		return current_listener->debug_id;
	}

}




