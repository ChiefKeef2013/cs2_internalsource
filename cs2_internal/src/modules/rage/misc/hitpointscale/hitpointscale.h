#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/threadpool/threadpool.cpp>
#include <core/features.hpp>

namespace features::combat {

	const std::vector<math::vector3>& rage::generate_multipoints( const systems::hitboxes::entry& hitbox, const math::vector3& center, const math::quaternion& bone_rot, float pointscale, const math::vector3& shoot_pos, float inaccuracy ) const
	{
		thread_local std::vector<math::vector3> out;
		out.clear( );

		auto scale = std::clamp( pointscale / 100.0f, 0.0f, 1.0f );
		if ( scale <= 0.01f )
		{
			return out;
		}

		const auto hb_mid   = ( hitbox.mins + hitbox.maxs ) * 0.5f;
		const auto capsule_a = center + bone_rot.rotate_vector( hitbox.mins - hb_mid );
		const auto capsule_b = center + bone_rot.rotate_vector( hitbox.maxs - hb_mid );

		const auto& config = settings::g_combat.m_ragebot.get_group( g_shared.ctx( ).weapon_type, g_shared.ctx( ).item_def_idx );
		if ( config.dynamic_pointscale.value && hitbox.radius > 0.001f )
		{
			const auto cone = std::max( inaccuracy + g_shared.ctx( ).spread, 0.0f );
			const auto cone_radius = std::tanf( cone ) * ( center - shoot_pos ).length( );
			const auto automatic_scale = std::clamp( 0.9f - cone_radius / hitbox.radius, 0.0f, 1.0f );
			scale = std::min( scale, automatic_scale );

			if ( scale <= 0.01f )
			{
				return out;
			}
		}

		const auto shoot_dir = ( center - shoot_pos ).normalized( );
		const auto ang       = math::helpers::vector_to_angle( shoot_dir );

		math::vector3 left{}, up{};
		math::helpers::angle_vectors_left( ang, nullptr, &left, &up );

		const auto right = math::vector3{ -left.x, -left.y, -left.z };

		const auto surface_point = [ & ]( const math::vector3& direction ) -> math::vector3
		{
			const auto dir = direction.normalized( );

			if ( hitbox.radius > 0.001f )
			{
				const auto reach = ( capsule_b - capsule_a ).length( ) + hitbox.radius * 2.0f + 1.0f;
				const auto origin = center + dir * reach;
				const auto delta = dir * ( reach * -2.0f );
				auto fraction{ 1.0f };

				if ( g_shared.ray_vs_capsule( origin, delta, capsule_a, capsule_b, hitbox.radius, fraction ) )
				{
					return origin + delta * fraction;
				}
			}
			else
			{

				auto inverse = bone_rot;
				inverse.x = -inverse.x;
				inverse.y = -inverse.y;
				inverse.z = -inverse.z;
				const auto local_dir = inverse.rotate_vector( dir );
				const auto extents = ( hitbox.maxs - hitbox.mins ) * 0.5f;
				auto distance = 8192.0f;

			if ( std::fabs( local_dir.x ) > 1.0e-6f ) distance = std::min( distance, std::fabs( extents.x / local_dir.x ) );
				if ( std::fabs( local_dir.y ) > 1.0e-6f ) distance = std::min( distance, std::fabs( extents.y / local_dir.y ) );
				if ( std::fabs( local_dir.z ) > 1.0e-6f ) distance = std::min( distance, std::fabs( extents.z / local_dir.z ) );

				if ( distance < 8192.0f )
				{
					return center + dir * distance;
				}
			}

			return center;
		};

		const auto scaled_surface = [ & ]( const math::vector3& direction )
		{
			const auto surface = surface_point( direction );
			return center + ( surface - center ) * scale;
		};

		switch ( hitbox.index )
		{
		case 0:
		{
			out.reserve( 8 );
			out.push_back( scaled_surface( right ) );
			out.push_back( scaled_surface( -right ) );
			out.push_back( scaled_surface( up ) );
			out.push_back( scaled_surface( -up ) );

			if ( config.dynamic_pointscale.value )
			{
				const auto diagonal = 0.70710678f;
				out.push_back( scaled_surface( right * diagonal + up * diagonal ) );
				out.push_back( scaled_surface( right * diagonal - up * diagonal ) );
				out.push_back( scaled_surface( -right * diagonal + up * diagonal ) );
				out.push_back( scaled_surface( -right * diagonal - up * diagonal ) );
			}
			break;
		}

		case 2: case 3:
		{
			out.reserve( 4 );
			out.push_back( scaled_surface( right ) );
			out.push_back( scaled_surface( -right ) );

			if ( config.dynamic_pointscale.value )
			{
				out.push_back( scaled_surface( up ) );
				out.push_back( scaled_surface( -up ) );
			}
			break;
		}

		case 4: case 5: case 6:
		{
			out.reserve( 3 );
			out.push_back( scaled_surface( right ) );
			out.push_back( scaled_surface( -right ) );
			if ( hitbox.index == 6 )
			{
				out.push_back( scaled_surface( up ) );
			}
			break;
		}

		case 7: case 8: case 9: case 10: case 11: case 12:
		{
			out.reserve( 2 );
			out.push_back( capsule_a );
			out.push_back( capsule_b );
			break;
		}

		case 13: case 14: case 15: case 16: case 17: case 18:
		{
			out.reserve( 1 );
			out.push_back( capsule_b );
			break;
		}

		default:
		{
			out.reserve( 2 );
			out.push_back( scaled_surface( right ) );
			out.push_back( scaled_surface( -right ) );
			break;
		}
		}

		return out;
	}

}


