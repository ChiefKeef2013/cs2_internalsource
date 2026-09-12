#pragma once

#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {

	void scene::apply_worldblur_param( __m128i*& value, std::uint32_t hash ) const
	{
		constexpr std::uint32_t dof_ranges{ 0x2ACAB07C };
		if ( !settings::g_world.m_scene.dof.value || hash != dof_ranges )
			return;

		static __m128 dof_value;
		const auto& scene_cfg = settings::g_world.m_scene;
		dof_value = _mm_set_ps(
			scene_cfg.dof_far_blurry.value,
			scene_cfg.dof_far_crisp.value,
			scene_cfg.dof_near_crisp.value,
			scene_cfg.dof_near_blurry.value );
		value = reinterpret_cast< __m128i* >( &dof_value );
	}

}
