#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {

	namespace {

		[[nodiscard]] bool is_finite_angles( const math::vector3& angles )
		{
			return std::isfinite( angles.x ) && std::isfinite( angles.y ) && std::isfinite( angles.z );
		}

		[[nodiscard]] float normalize_angle( float angle )
		{
			while ( angle > 180.0f )
			{
				angle -= 360.0f;
			}

			while ( angle < -180.0f )
			{
				angle += 360.0f;
			}

			return angle;
		}

		[[nodiscard]] math::vector3 build_offset( const math::vector3& camera_angles )
		{
			const auto& cfg = settings::g_misc.m_viewmodel_adjust;
			math::vector3 forward{}, left{}, up{};
			math::helpers::angle_vectors_left( camera_angles, &forward, &left, &up );
			const auto right = -left;

			return ( right * cfg.offset_x.value ) +
				( forward * cfg.offset_y.value ) +
				( up * cfg.offset_z.value );
		}

	}

	void viewmodel::apply( math::vector3* position, math::vector3* angles ) const
	{
		const auto& cfg = settings::g_misc.m_viewmodel_adjust;
		if ( !cfg.enabled.value || !angles || !is_finite_angles( *angles ) )
		{
			return;
		}

	if ( position && std::isfinite( position->x ) && std::isfinite( position->y ) && std::isfinite( position->z ) )
		{
			const auto offset = build_offset( *angles );
			position->x += offset.x;
			position->y += offset.y;
			position->z += offset.z;
		}

		angles->x = normalize_angle( angles->x + cfg.pitch.value );
		angles->y = normalize_angle( angles->y + cfg.yaw.value );
		angles->z = normalize_angle( angles->z + cfg.roll.value );
	}

}

