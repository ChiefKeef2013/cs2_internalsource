#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {

	void reveal_radar::on_frame_stage_notify( ) const
	{
		if ( !settings::g_misc.reveal_radar.value )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_valid( ) )
		{
			return;
		}

		const auto spotted_state_offset = SCHEMA_OFFSET( "C_CSPlayerPawn", "m_entitySpottedState"_hash );
		const auto spotted_offset = SCHEMA_OFFSET( "EntitySpottedState_t", "m_bSpotted"_hash );

		for ( const auto& player : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
			const auto controller = player.ptr;
			if ( !controller || !reinterpret_cast< CCSPlayerController* >( controller )->m_bPawnIsAlive( ) )
			{
				continue;
			}

			const auto pawn = systems::g_entities.player_pawn( controller );
			if ( !pawn || pawn == local.view_pawn( ) )
			{
				continue;
			}

			const auto team = reinterpret_cast< C_BaseEntity* >( pawn )->m_iTeamNum( );
			if ( !local.is_this_other_team( team ) )
			{
				continue;
			}

			memory::write<bool>( pawn + spotted_state_offset + spotted_offset, true );
		}
	}

}

