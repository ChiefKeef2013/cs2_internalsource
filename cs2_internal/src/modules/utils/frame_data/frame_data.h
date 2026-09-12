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

    void frame_data::update( )
    {
		if ( systems::g_lifecycle.busy( ) )
		{
			this->reset( );
			return;
		}

        const auto local = systems::g_local.get( );
        if ( !local.is_alive || !memory::is_game_ptr( local.pawn ) )
        {
            this->reset( );
            return;
        }

        const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( local.pawn )->m_pGameSceneNode();
        if ( !memory::is_game_ptr( game_scene_node ) )
        {
            this->reset( );
            return;
        }

        this->m_origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin();
        this->m_valid = true;
    }

	void frame_data::reset( )
	{
		this->m_origin = {};
		this->m_valid = false;
	}

}



