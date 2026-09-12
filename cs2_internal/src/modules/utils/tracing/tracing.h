#pragma once

#include <core/common.hpp>
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

namespace systems {

	namespace {

		[[nodiscard]] bool finite_vec( const math::vector3& v )
		{
			return std::isfinite( v.x ) && std::isfinite( v.y ) && std::isfinite( v.z );
		}

		[[nodiscard]] bool traces_ready( )
		{
			return addresses::globals::game_trace_manager != 0;
		}

	}

	bool tracing::is_visible( const math::vector3& start, const math::vector3& end, std::uintptr_t target_entity, std::uintptr_t skip_entity, std::uintptr_t mask ) const
	{
		auto current_start = start;
		auto entity_to_skip = skip_entity;

		constexpr auto max_penetrations{ 3 };

		for ( auto i = 0; i < max_penetrations; ++i )
		{
			const auto result = this->trace( current_start, end, entity_to_skip, mask );

			if ( result.hit_entity == target_entity || result.fraction > 0.97f )
			{
				return true;
			}

			if ( !result.hit_entity )
			{
				break;
			}

			const auto hit_health = reinterpret_cast<C_BaseEntity*>( result.hit_entity )->m_iHealth();
			if ( hit_health > 0 && hit_health <= 100 )
			{
				entity_to_skip = result.hit_entity;
				current_start = result.end_pos + ( end - current_start ).normalized( );
				continue;
			}

			break;
		}

		return false;
	}

	tracing::result tracing::trace( const math::vector3& start, const math::vector3& end, std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer ) const
	{
		auto filter = this->make_filter( skip_entity, mask, layer );
		return this->trace( start, end, filter );
	}

	tracing::result tracing::trace( const math::vector3& start, const math::vector3& end, const filter& filter ) const
	{
		result result{};

		if ( !traces_ready( ) ||
			!finite_vec( start ) || !finite_vec( end ) )
		{
			return result;
		}

		ray ray{};
		memory::call<bool>(PATTERN(PATTERN_TRACE_RAY), addresses::globals::game_trace_manager, &ray, &start, &end, &filter, &result );

		return result;
	}

	tracing::result tracing::trace_hull( const math::vector3& start, const math::vector3& end, const math::vector3& mins, const math::vector3& maxs, std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer ) const
	{
		const auto filter = this->make_filter( skip_entity, mask, layer );
		return this->trace_hull( start, end, mins, maxs, filter );
	}

	tracing::result tracing::trace_hull( const math::vector3& start, const math::vector3& end, const math::vector3& mins, const math::vector3& maxs, const filter& filter ) const
	{
		result result{};

		if ( !traces_ready( ) ||
			!finite_vec( start ) || !finite_vec( end ) ||
			!finite_vec( mins ) || !finite_vec( maxs ) )
		{
			return result;
		}

		ray ray{};
		ray.mins = mins;
		ray.maxs = maxs;
		ray.type = 2;

		memory::call<bool>(PATTERN(PATTERN_TRACE_RAY), addresses::globals::game_trace_manager, &ray, &start, &end, &filter, &result );

		return result;
	}

	tracing::result tracing::trace_sphere( const math::vector3& start, const math::vector3& end, float radius, const filter& filter ) const
	{
		result result{};

		if ( !traces_ready( ) || !finite_vec( start ) || !finite_vec( end ) || !std::isfinite( radius ) )
		{
			return result;
		}

		ray ray{};
		ray.mins = {};
		*reinterpret_cast< float* >( reinterpret_cast< std::uintptr_t >( &ray ) + 12 ) = radius;
		ray.type = 1;

		memory::call<bool>(PATTERN(PATTERN_TRACE_RAY), addresses::globals::game_trace_manager, &ray, &start, &end, &filter, &result );

		return result;
	}

	tracing::result tracing::trace_to_entity( const math::vector3& start, const math::vector3& end, std::uintptr_t target_entity, std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer ) const
	{
		const auto filter = this->make_filter( skip_entity, mask, layer );
		return this->trace_to_entity( start, end, target_entity, filter );
	}

	tracing::result tracing::trace_to_entity( const math::vector3& start, const math::vector3& end, std::uintptr_t target_entity, const filter& filter ) const
	{
		result result{};

		if ( !traces_ready( ) || !memory::is_game_ptr( target_entity ) || !finite_vec( start ) || !finite_vec( end ) )
		{
			return result;
		}

		ray ray{};
		memory::call<bool>(PATTERN(PATTERN_TRACE_RAY_ENTITY), addresses::globals::game_trace_manager, &ray, &start, &end, target_entity, &filter, &result );

		return result;
	}

	tracing::filter tracing::make_filter( std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer, int type ) const
	{
		filter filter{};

		const auto skip = memory::is_game_ptr( skip_entity ) ? skip_entity : 0;
		memory::call<void>(PATTERN(PATTERN_TRACE_FILTER_INIT), &filter, skip, mask, layer, type );

		return filter;
	}

	tracing::filter tracing::make_filter( std::uintptr_t skip_entity, std::uintptr_t mask, std::uint8_t layer ) const
	{
		return this->make_filter( skip_entity, mask, layer, 7 );
	}

	tracing::player_movement_filter tracing::make_player_movement_filter( std::uintptr_t entity, std::uintptr_t mask, std::uint8_t collision_group ) const
	{
		player_movement_filter filter{};

		const auto skip = memory::is_game_ptr( entity ) ? entity : 0;
		memory::call<void>(PATTERN(PATTERN_TRACE_FILTER_SET_COLLISION), &filter, skip, mask, static_cast< int >( collision_group ) );

		return filter;
	}

	tracing::result tracing::trace_player_bbox( const math::vector3& start, const math::vector3& end, const bbox_collision& bbox, const player_movement_filter& filter, std::uintptr_t movement_services ) const
	{
		result result{};

		if ( !memory::is_game_ptr( movement_services ) || !finite_vec( start ) || !finite_vec( end ) )
		{
			return result;
		}

		memory::call<void>(PATTERN(PATTERN_TRACE_HULL), movement_services + 1968, &result, &start, &end, &bbox, &filter );

		return result;
	}

	void tracing::setup_trace( trace_data* trace_data, const math::vector3& start, const math::vector3& delta, const filter& filter, int penetration_count, bool trace_world ) const
	{
		if ( !trace_data || !traces_ready( ) || !finite_vec( start ) || !finite_vec( delta ) )
		{
			return;
		}

		memory::call<void>(PATTERN(PATTERN_TRACE_BULLET_DATA_INIT), trace_data, start, delta, filter, penetration_count, trace_world );
	}

	void tracing::init_result( result* trace_result ) const
	{
		if ( !trace_result )
		{
			return;
		}

		memory::call<void>(PATTERN(PATTERN_TRACE_BULLET_FREE), trace_result );
	}

	void tracing::finalize_trace( trace_data* trace_data, result* hit, float unknown_float, void* unknown ) const
	{
		if ( !trace_data || !hit )
		{
			return;
		}

		memory::call<void>(PATTERN(PATTERN_TRACE_BULLET_UPDATE), trace_data, hit, unknown_float, unknown );
	}

}





