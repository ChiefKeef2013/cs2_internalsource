#pragma once

#include <modules/visuals/worldmodulation/world.hpp>
#include <core/patterns.h>

namespace features::world {

	bool scene::on_setup_fog( __m128i* output, int* mode ) const
	{
		const auto& fog = settings::g_world.m_weather;
		if ( !fog.fog_enabled.value || !output || !mode )
			return false;

		const auto set_param_f = PATTERN( PATTERN_SET_SHADER_PARAM );
		const auto set_param_i = PATTERN( PATTERN_SET_SHADER_PARAM_I );
		if ( !set_param_f || !set_param_i )
			return false;

		const auto falloff = 0.5f + fog.fog_anisotropy.value * 8.0f;
		const auto& color = fog.fog_color.value;
		const __m128 params = _mm_set_ps( 0.0f, 0.0f, fog.fog_draw_distance.value, 0.0f );
		const __m128 params_2 = _mm_set_ps( 0.0f, 0.0f, falloff, fog.fog_density.value );
		const __m128 params_3 = _mm_set_ps(
			0.0f,
			static_cast< float >( color.b ) / 255.0f,
			static_cast< float >( color.g ) / 255.0f,
			static_cast< float >( color.r ) / 255.0f );

		memory::call<std::uintptr_t>( set_param_f, output, detail::shader_hash::gradient_fog, &params );
		memory::call<std::uintptr_t>( set_param_f, output, detail::shader_hash::gradient_fog_2, &params_2 );
		memory::call<std::uintptr_t>( set_param_f, output, detail::shader_hash::gradient_fog_3, &params_3 );
		memory::call<std::uintptr_t>( set_param_i, output + 17, detail::shader_hash::enable_gradient_fog, 1 );
		*mode = 0;
		return true;
	}

}
