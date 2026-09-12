#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>

namespace features::combat {

	namespace {

		constexpr auto k_pen_crosshair_half_size{ 3.5f };
		constexpr auto k_pen_crosshair_surface_push{ 0.05f };

		[[nodiscard]] bool pen_crosshair_weapon_ok( const shared::context& ctx, const systems::local::snapshot& local )
		{
			return ctx.valid
				&& local.is_alive
				&& local.team >= 2
				&& local.pawn
				&& ctx.weapon
				&& ctx.weapon_type >= cstypes::weapon_type::pistol
				&& ctx.weapon_type <= cstypes::weapon_type::lmg
				&& !systems::g_local.is_in_cinematic( );
		}

		[[nodiscard]] math::vector3 pen_crosshair_basis_axis( const math::vector3& normal, const math::vector3& hint )
		{
			auto axis = hint - normal * hint.dot( normal );
			if ( axis.length_sqr( ) < 1.0e-6f )
			{
				return {};
			}

			return axis.normalized( );
		}

	}

	void rage::sample_penetration_crosshair( const systems::local::snapshot& local )
	{
		const auto& cfg = settings::g_combat.m_penetration_crosshair;
		const auto& ctx = g_shared.ctx( );

		if ( !cfg.enabled.value || !pen_crosshair_weapon_ok( ctx, local ) )
		{
			this->m_pen_can_pen.store( false, std::memory_order_release );
			return;
		}

		auto view_angles = systems::g_input.get_view_angles( );
		const auto aim_punch = g_shared.get_aim_punch( local.pawn );
		view_angles.x += aim_punch.x;
		view_angles.y += aim_punch.y;
		math::helpers::normalize_angles( view_angles );

		math::vector3 forward{};
		math::helpers::angle_vectors_left( view_angles, &forward );
		if ( forward.length_sqr( ) < 1.0e-8f )
		{
			this->m_pen_can_pen.store( false, std::memory_order_release );
			return;
		}

		float pen_damage{};
		this->m_pen_can_pen.store( g_shared.pen( ).can( g_shared.get_eye_position( local.pawn ), forward, pen_damage, local ), std::memory_order_release );
	}

	void rage::update_penetration_crosshair( const systems::local::snapshot& local )
	{
		const auto& cfg = settings::g_combat.m_penetration_crosshair;
		const auto& ctx = g_shared.ctx( );

		if ( !cfg.enabled.value || !pen_crosshair_weapon_ok( ctx, local ) || !systems::g_view.has_camera( ) )
		{
			this->m_penetration_crosshair = {};
			return;
		}

		auto eye_pos = systems::g_view.origin( );
		auto view_angles = systems::g_view.angles( );
		if ( !std::isfinite( eye_pos.x ) || !std::isfinite( view_angles.x ) || systems::g_view.fov( ) > 180.0f || systems::g_view.fov( ) < 1.0f )
		{
			eye_pos = g_shared.get_eye_position( local.pawn );
			view_angles = systems::g_input.get_view_angles( );
		}

		math::helpers::normalize_angles( view_angles );

		math::vector3 forward{};
		math::helpers::angle_vectors_left( view_angles, &forward );
		if ( forward.length_sqr( ) < 1.0e-8f )
		{
			this->m_penetration_crosshair = {};
			return;
		}

		const auto range = ctx.range > 1.0f ? ctx.range : 8192.0f;
		const auto filter = systems::g_tracing.make_filter( local.pawn, k_pen_crosshair_brush_mask, 4 );
		const auto wall = systems::g_tracing.trace( eye_pos, eye_pos + forward * range, filter );

		if ( wall.fraction >= 1.0f || wall.fraction <= 0.0f || !std::isfinite( wall.end_pos.x ) || !std::isfinite( wall.normal.x ) )
		{
			this->m_penetration_crosshair = {};
			return;
		}

		auto normal = wall.normal;
		const auto n_len_sq = normal.length_sqr( );
		if ( n_len_sq < 1.0e-8f )
		{
			this->m_penetration_crosshair = {};
			return;
		}

		normal *= 1.0f / std::sqrt( n_len_sq );
		if ( normal.dot( forward ) > 0.0f )
		{
			normal = -normal;
		}

		pen_crosshair_result out{};
		out.hit = true;
		out.position = eye_pos + forward * ( wall.fraction * range );
		out.normal = normal;
		out.can_pen = this->m_pen_can_pen.load( std::memory_order_acquire );
		this->m_penetration_crosshair = out;
	}

	void rage::draw_penetration_crosshair( xdraw::draw_list& draw_list ) const
	{
		const auto& cfg = settings::g_combat.m_penetration_crosshair;
		if ( !cfg.enabled.value )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || systems::g_local.is_in_cinematic( ) )
		{
			return;
		}

		const auto& result = this->m_penetration_crosshair;
		if ( !result.hit )
		{
			return;
		}

		math::vector3 left{}, up{};
		math::helpers::angle_vectors_left( systems::g_view.angles( ), nullptr, &left, &up );

		auto tangent = pen_crosshair_basis_axis( result.normal, left );
		if ( tangent.length_sqr( ) < 1.0e-8f )
		{
			tangent = pen_crosshair_basis_axis( result.normal, up );
		}

		if ( tangent.length_sqr( ) < 1.0e-8f )
		{
			return;
		}

		const auto bitangent = result.normal.cross( tangent );
		const auto center = result.position + result.normal * k_pen_crosshair_surface_push;

		const math::vector3 corners[ 4 ]
		{
			center - tangent * k_pen_crosshair_half_size - bitangent * k_pen_crosshair_half_size,
			center + tangent * k_pen_crosshair_half_size - bitangent * k_pen_crosshair_half_size,
			center + tangent * k_pen_crosshair_half_size + bitangent * k_pen_crosshair_half_size,
			center - tangent * k_pen_crosshair_half_size + bitangent * k_pen_crosshair_half_size,
		};

		float sx[ 5 ]{}, sy[ 5 ]{};
		for ( auto i = 0; i < 4; ++i )
		{
			const auto proj = systems::g_view.project( corners[ i ] );
			if ( !systems::g_view.projection_valid( proj ) )
			{
				return;
			}

			sx[ i ] = proj.x;
			sy[ i ] = proj.y;
		}

		const auto center_proj = systems::g_view.project( center );
		if ( !systems::g_view.projection_valid( center_proj ) )
		{
			return;
		}

		sx[ 4 ] = center_proj.x;
		sy[ 4 ] = center_proj.y;

		const auto fill = result.can_pen ? cfg.can_penetrate.value : cfg.blocked.value;
		const auto edge = fill.alpha( static_cast< std::uint8_t >( fill.a / 4 ) );
		const auto outline = fill.alpha( 255 );

		for ( auto i = 0; i < 4; ++i )
		{
			const auto j = ( i + 1 ) % 4;
			draw_list.triangle_filled_multi(
				sx[ 4 ], sy[ 4 ], fill,
				sx[ i ], sy[ i ], edge,
				sx[ j ], sy[ j ], edge );
		}

		const float screen[ 8 ]{ sx[ 0 ], sy[ 0 ], sx[ 1 ], sy[ 1 ], sx[ 2 ], sy[ 2 ], sx[ 3 ], sy[ 3 ] };
		draw_list.polyline( std::span<const float>( screen, 8 ), outline, true, 1.0f );
	}

}
