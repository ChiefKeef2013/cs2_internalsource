#pragma once

#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {

	void scene::on_light_scene_object_pre( std::uintptr_t object ) const
	{
		const auto& scene_cfg = settings::g_world.m_scene;
		const auto night = scene_cfg.night_mode.value;
		if ( !memory::is_game_ptr( object ) || ( !scene_cfg.lighting.value && !night ) )
			return;

		auto intensity = scene_cfg.lighting.value ? scene_cfg.lighting_intensity.value : 1.0f;
		auto color = scene_cfg.lighting.value
			? scene_cfg.lighting_color.value.to_float( )
			: std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f };

		if ( night )
		{
			intensity *= this->get_night_exposure_scale( );
			const auto ambient = scene_cfg.night_ambient.value.to_float( );
			color[ 0 ] *= ambient[ 0 ];
			color[ 1 ] *= ambient[ 1 ];
			color[ 2 ] *= ambient[ 2 ];
		}

		constexpr std::uintptr_t k_data_offset = 0xE0;
		constexpr std::uintptr_t k_type = k_data_offset + 0x0;
		constexpr std::uintptr_t k_color = k_data_offset + 0x4;
		constexpr std::uintptr_t k_shadows = k_data_offset + 0x3D;
		constexpr std::uintptr_t k_shadow_slot = k_data_offset + 0x74;
		constexpr std::uintptr_t k_enabled = k_data_offset + 0x75;
		constexpr std::uintptr_t k_rot_x = k_data_offset + 0xA4;
		constexpr std::uintptr_t k_rot_y = k_data_offset + 0xA8;
		constexpr std::uintptr_t k_alpha = k_data_offset + 0xAC;

		constexpr std::uint32_t k_directional_light = 2u;
		const auto light_type = memory::read<std::uint32_t>( object + k_type );
		const auto is_directional = ( light_type == k_directional_light );

		memory::write<float>( object + k_color + 0x0, color[ 0 ] * intensity );
		memory::write<float>( object + k_color + 0x4, color[ 1 ] * intensity );
		memory::write<float>( object + k_color + 0x8, color[ 2 ] * intensity );

		if ( !is_directional || !scene_cfg.lighting.value )
			return;

		if ( scene_cfg.lighting_disable.value )
		{
			memory::write<std::uint8_t>( object + k_shadow_slot, 0xFFu );
			memory::write<bool>( object + k_enabled, false );
			memory::write<float>( object + k_alpha, 0.0f );
			memory::write<float>( object + k_color + 0x0, 0.0f );
			memory::write<float>( object + k_color + 0x4, 0.0f );
			memory::write<float>( object + k_color + 0x8, 0.0f );
			return;
		}

		memory::write<bool>( object + k_shadows, scene_cfg.lighting_shadows.value );
		memory::write<std::uint8_t>( object + k_shadow_slot, scene_cfg.lighting_baked_shadows.value ? 0u : 1u );
		memory::write<bool>( object + k_enabled, true );
		memory::write<float>( object + k_alpha, 1.0f );

		if ( scene_cfg.lighting_change_rotation.value )
		{
			memory::write<float>( object + k_rot_x, scene_cfg.lighting_rot_x.value );
			memory::write<float>( object + k_rot_y, scene_cfg.lighting_rot_y.value );
		}
	}

	void scene::on_light_scene_object_post( std::uintptr_t object ) const
	{
		( void )object;
	}

}
