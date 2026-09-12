#pragma once

#include <core/common.hpp>
#include <core/memory.hpp>

#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerController.h>
#include <valve/schemas/CBasePlayerPawn.h>
#include <valve/schemas/CCollisionProperty.h>
#include <valve/schemas/CCSGameRules.h>
#include <valve/schemas/CEconEntity.h>
#include <valve/schemas/CGameSceneNode.h>
#include <valve/schemas/CGrenade.h>
#include <valve/schemas/CPlantedC4.h>
#include <valve/schemas/CWeapon.h>

namespace systems {

	void local::update( )
	{
		if ( systems::g_lifecycle.busy( ) )
		{
			this->reset( );
			return;
		}

		const auto local_player_controller = memory::read<std::uintptr_t>( addresses::globals::local_player_controller );
		if ( !memory::is_game_ptr( local_player_controller ) )
		{
			this->reset( );
			return;
		}

		snapshot s{};
		s.controller = local_player_controller;

		const auto pawn = g_entities.player_pawn( local_player_controller );
		if ( memory::is_game_ptr( pawn ) )
		{
			s.pawn = pawn;
			s.team = reinterpret_cast<C_BaseEntity*>( pawn )->m_iTeamNum();

			const auto health = reinterpret_cast<C_BaseEntity*>( pawn )->m_iHealth();
			const auto life_state = reinterpret_cast<C_BaseEntity*>( pawn )->m_lifeState();
			s.is_alive = reinterpret_cast<CCSPlayerController*>( local_player_controller )->m_bPawnIsAlive( )
				&& health > 0 && life_state == 0;
		}

		if ( s.is_alive )
		{
			s.view_team = s.team;
		}
		else
		{
			const auto observer_pawn = g_entities.observer_pawn( local_player_controller );
			const auto observer_target = g_entities.observer_target( observer_pawn );
			if ( observer_target )
			{
				s.observer_pawn = observer_target;
				s.view_team = memory::read<int>(
					observer_target + SCHEMA_OFFSET( "C_BaseEntity", "m_iTeamNum"_hash ) );

				const auto observer_target_controller_handle = memory::read<std::uint32_t>(
					observer_target + SCHEMA_OFFSET( "C_BasePlayerPawn", "m_hController"_hash ) );
				const auto observer_target_controller = g_entities.lookup( observer_target_controller_handle );
				if ( memory::is_game_ptr( observer_target_controller ) )
				{
					s.observer_controller = observer_target_controller;
				}
			}

			if ( !s.observer_pawn )
			{
				s.view_team = s.team;
			}
		}

		{
			const auto game_type_cvar = CONVAR( "game_type" );
			const auto game_mode_cvar = CONVAR( "game_mode" );
			if ( game_type_cvar && game_mode_cvar )
			{
				const auto game_type = game_type_cvar->get<int>( );
				const auto game_mode = game_mode_cvar->get<int>( );
				const auto is_ffa = ( game_type == 1 && game_mode == 2 ) || ( game_type == 2 && game_mode == 0 );

				s.is_team_mode = !is_ffa;
				this->m_is_deathmatch.store( game_type == 1 && game_mode == 2 );
			}
			else
			{
				s.is_team_mode = true;
				this->m_is_deathmatch.store( false );
			}
		}

		{
			const auto game_rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
			const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );

			auto cinematic{ false };
			auto freezetime{ false };

			if ( memory::is_game_ptr( game_rules ) && memory::is_game_ptr( global_vars ) )
			{
				const auto rules = reinterpret_cast<C_CSGameRules*>( game_rules );
				if ( rules->m_bTeamIntroPeriod() )
				{
					cinematic = true;
				}
				else if ( rules->m_gamePhase() >= 4 )
				{
					cinematic = true;
				}

				if ( rules->m_bFreezePeriod() )
				{
					freezetime = true;
				}
				else
				{
					const auto round_start_time = rules->m_fRoundStartTime();
					const auto current_time = memory::read<float>( global_vars + 0x30 );

					if ( round_start_time > current_time )
					{
						freezetime = true;
					}
				}
			}

			this->m_is_in_cinematic.store( cinematic );
			this->m_is_in_time_freeze.store( freezetime );
		}

		this->m_seq.fetch_add( 1, std::memory_order_relaxed );
		std::atomic_thread_fence( std::memory_order_release );
		this->m_snapshot = s;
		std::atomic_thread_fence( std::memory_order_release );
		this->m_seq.fetch_add( 1, std::memory_order_release );
	}

	void local::reset( )
	{
		this->m_seq.fetch_add( 1, std::memory_order_relaxed );
		std::atomic_thread_fence( std::memory_order_release );
		this->m_snapshot = {};
		std::atomic_thread_fence( std::memory_order_release );
		this->m_seq.fetch_add( 1, std::memory_order_release );

		this->m_is_deathmatch.store( false );
		this->m_is_in_cinematic.store( false );
		this->m_is_in_time_freeze.store( false );
	}

}





