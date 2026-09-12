#pragma once

#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {

	void scene::apply_bloom_param( __m128i*& value, std::uint32_t hash ) const
	{
		constexpr std::uint32_t scale_hash{ 0x565EAF76u };
		constexpr std::uint32_t threshold_hash{ 0xBA98A9B0u };
		constexpr std::uint32_t width_hash{ 0x2AE72B37u };
		constexpr std::uint32_t strength_hash{ 0xB692902Eu };
		constexpr std::uint32_t skybox_hash{ 0x1313A424u };

		const auto& scene_cfg = settings::g_world.m_scene;
		if ( !scene_cfg.bloom.value )
			return;

		static __m128 scale_value;
		static __m128 threshold_value;
		static __m128 width_value;
		static __m128 strength_value;
		static __m128 skybox_value;

		const auto amount = std::clamp( scene_cfg.bloom_value.value, 0.0f, 2.0f );
		switch ( hash )
		{
		case scale_hash:
			scale_value = _mm_set_ps1( 0.3f + amount * 1.2f );
			value = reinterpret_cast< __m128i* >( &scale_value );
			break;
		case threshold_hash:
			threshold_value = _mm_set_ps1( 1.5f - amount * 1.2f );
			value = reinterpret_cast< __m128i* >( &threshold_value );
			break;
		case width_hash:
			width_value = _mm_set_ps1( 0.5f + amount * 1.5f );
			value = reinterpret_cast< __m128i* >( &width_value );
			break;
		case strength_hash:
			strength_value = _mm_set_ps1( 0.2f + amount * 0.6f );
			value = reinterpret_cast< __m128i* >( &strength_value );
			break;
		case skybox_hash:
			skybox_value = _mm_set_ps1( 0.1f + amount * 0.4f );
			value = reinterpret_cast< __m128i* >( &skybox_value );
			break;
		default:
			break;
		}
	}

}
