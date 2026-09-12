#pragma once

#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {

	void scene::apply_wetness_param( __m128i*& value, std::uint32_t hash ) const
	{
		static __m128 sky_value;
		static __m128 wetness_value;
		static __m128 timer_value;

		const auto& weather = settings::g_world.m_weather;
		const auto enabled = weather.wetness.value;

		switch ( hash )
		{
		case detail::shader_hash::rain_exposure_to_sky:
			sky_value = _mm_set_ps1( enabled ? 1.0f : 0.0f );
			value = reinterpret_cast< __m128i* >( &sky_value );
			break;
		case detail::shader_hash::rain_wetness:
			wetness_value = _mm_set_ps1( enabled
				? std::clamp( weather.wetness_density.value, 0.0f, 5.0f )
				: 0.0f );
			value = reinterpret_cast< __m128i* >( &wetness_value );
			break;
		case detail::shader_hash::rain_timer:
		{
			const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );
			const auto current_time = global_vars
				? memory::read<float>( global_vars + 0x30 )
				: static_cast< float >( GetTickCount64( ) ) * 0.001f;

			timer_value = _mm_set_ps1( enabled
				? current_time * std::clamp( weather.wetness_speed.value, 0.0f, 3.0f )
				: 0.0f );
			value = reinterpret_cast< __m128i* >( &timer_value );
			break;
		}
		default:
			break;
		}
	}

}
