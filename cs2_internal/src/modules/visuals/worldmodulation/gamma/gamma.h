#pragma once

#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {

	void scene::apply_gamma_param( __m128i*& value, std::uint32_t hash ) const
	{
		constexpr std::uint32_t gamma_hash{ 0x24470A87u };
		if ( !settings::g_world.m_scene.gamma.value || hash != gamma_hash )
			return;

		static __m128 gamma_value;
		gamma_value = _mm_set_ps1(
			std::clamp( settings::g_world.m_scene.gamma_value.value, 0.5f, 5.0f ) );
		value = reinterpret_cast< __m128i* >( &gamma_value );
	}

}
