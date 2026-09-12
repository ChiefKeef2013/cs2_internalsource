#pragma once

#include <core/common.hpp>
#include <cmath>
#include <core/memory.hpp>
#include <core/common.hpp>

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

#include <core/common.hpp>

namespace systems {

	bounds::data bounds::get( std::uintptr_t entity )
	{
		if ( !memory::is_game_ptr( entity ) )
		{
			return {};
		}

		const auto collision = reinterpret_cast<C_BaseEntity*>( entity )->m_pCollision();
		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( entity )->m_pGameSceneNode();

		if ( !memory::is_game_ptr( collision ) || !memory::is_game_ptr( game_scene_node ) )
		{
			return {};
		}

		const auto origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin();
		const auto mins = reinterpret_cast<CCollisionProperty*>( collision )->m_vecMins() + origin;
		const auto maxs = reinterpret_cast<CCollisionProperty*>( collision )->m_vecMaxs() + origin;

		const auto [viewport_w, viewport_h] = xdraw::viewport_size( );
		const auto sw = static_cast< float >( viewport_w );
		const auto sh = static_cast< float >( viewport_h );

		data result{};
		result.min = { FLT_MAX, FLT_MAX };
		result.max = { -FLT_MAX, -FLT_MAX };

		constexpr auto edge_margin = 384.0f;
		auto had_valid_projection = false;

		for ( std::size_t i = 0; i < 8; ++i )
		{
			const math::vector3 corner
			{
				( i & 1 ) ? maxs.x : mins.x,
				( i & 2 ) ? maxs.y : mins.y,
				( i & 4 ) ? maxs.z : mins.z
			};

			const auto proj = g_view.project_full( corner );
			if ( proj.w <= 0.001f || !std::isfinite( proj.screen.x ) || !std::isfinite( proj.screen.y ) )
			{
				continue;
			}

			const auto sx = std::clamp( proj.screen.x, -edge_margin, sw + edge_margin );
			const auto sy = std::clamp( proj.screen.y, -edge_margin, sh + edge_margin );

			result.min.x = std::min( result.min.x, sx );
			result.min.y = std::min( result.min.y, sy );
			result.max.x = std::max( result.max.x, sx );
			result.max.y = std::max( result.max.y, sy );
			had_valid_projection = true;
		}

		if ( !had_valid_projection || result.min.x >= result.max.x || result.min.y >= result.max.y )
		{
			return {};
		}

		result.valid = true;
		return result;
	}

}




