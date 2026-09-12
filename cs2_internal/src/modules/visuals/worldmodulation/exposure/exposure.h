#pragma once

#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {

	float scene::get_night_exposure_scale( ) const
	{
		const auto& scene_cfg = settings::g_world.m_scene;
		const float t = std::clamp( scene_cfg.night_exposure.value / 20.0f, 0.0f, 1.0f );
		return 1.0f - t * 0.97f;
	}

}
