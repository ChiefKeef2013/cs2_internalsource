#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <modules/rage/misc/seed/hitchance.h>

namespace features::combat {

	float rage::evaluate_hitchance( const scan_hit& hit, const aim_context& ctx, float inaccuracy ) const
	{
		const auto& config = settings::g_combat.m_ragebot.get_group( g_shared.ctx( ).weapon_type, g_shared.ctx( ).item_def_idx );
		if ( config.no_spread.value || !hit.record || !hit.record->valid )
		{
			return config.no_spread.value ? 1.0f : 0.0f;
		}

		if ( g_shared.is_max_accuracy( inaccuracy ) )
		{
			return 1.0f;
		}

		if ( hit.bone_index < 0 || hit.bone_index >= 28 )
		{
			return 0.0f;
		}

		const auto& bone = hit.record->bones[ hit.bone_index ];
		return g_shared.calculate_hitchance(
			hit.source_eye.position,
			hit.aim_angle,
			hit.hitbox,
			bone,
			inaccuracy,
			ctx.spread );
	}

	bool shared::find_spread_correction( const math::vector3& aim_angle, int tick, math::vector3& out ) const
	{
		out = {};
		if ( tick <= 0 || !std::isfinite( aim_angle.x ) || !std::isfinite( aim_angle.y ) )
			return false;

		( void ) hitchance::init( );
		const int bullets = this->m_ctx.num_bullets > 0 ? this->m_ctx.num_bullets : 1;
		const int def = this->m_ctx.item_def_idx;
		const float inac = this->m_ctx.inaccuracy;
		const float spr = this->m_ctx.spread;
		if ( !std::isfinite( inac ) || !std::isfinite( spr ) )
			return false;

		const float base_pitch = aim_angle.x;
		for ( auto i = 0; i < 720; i++ )
		{
			const float offset = ( i == 0 ) ? 0.0f
				: ( ( i % 2 == 1 ) ? ( static_cast< float >( ( i + 1 ) / 2 ) * 0.5f )
				                   : ( -static_cast< float >( i / 2 ) * 0.5f ) );
			const float test_pitch = std::clamp( base_pitch + offset, -89.0f, 89.0f );

			const auto test_angles = math::vector3{ test_pitch, aim_angle.y, 0.0f };
			const auto seed = hitchance::spread_seed_ready( )
				? hitchance::compute_seed( test_angles, tick )
				: this->get_spread_seed( test_angles, tick );
			const auto spread = this->calculate_spread( seed, inac, spr, this->m_ctx.recoil_index, def, bullets );
			if ( !std::isfinite( spread.x ) || !std::isfinite( spread.y ) )
				continue;

			auto adj_angle = aim_angle;
			adj_angle.x += math::helpers::rad_to_deg( std::atan( std::sqrt( spread.x * spread.x + spread.y * spread.y ) ) );
			adj_angle.z = -math::helpers::rad_to_deg( std::atan2( spread.x, spread.y ) );
			if ( !std::isfinite( adj_angle.x ) || !std::isfinite( adj_angle.z ) )
				continue;

			const auto adj_seed = hitchance::spread_seed_ready( )
				? hitchance::compute_seed( adj_angle, tick )
				: this->get_spread_seed( adj_angle, tick );
			if ( adj_seed == seed )
			{
				out = adj_angle;
				return true;
			}
		}

		return false;
	}

}
