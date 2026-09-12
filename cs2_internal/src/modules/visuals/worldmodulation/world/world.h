#pragma once

#include <modules/visuals/worldmodulation/world.hpp>
#include <core/common.hpp>
#include <core/memory.hpp>

namespace features::world {

	namespace {

		[[nodiscard]] const char* engine_world_mat_path( settings::world::scene::world_engine_mat id ) noexcept
		{
			using mat = settings::world::scene::world_engine_mat;
			switch ( id )
			{
			case mat::reflectivity_90:
				return "materials/dev/reflectivity_90.vmat";
			case mat::metallic:
				return "materials/dev/metallic.vmat";
			case mat::primary_white:
			default:
				return "materials/dev/primary_white.vmat";
			}
		}

		[[nodiscard]] std::uintptr_t find_world_replacement( settings::world::scene::world_engine_mat id ) noexcept
		{
			using mat = settings::world::scene::world_engine_mat;
			if ( id == mat::metallic )
			{
				if ( const auto from_chams = systems::materials::find( settings::esp::cham_ids::metallic ) )
				{
					return from_chams;
				}
			}

			return systems::materials::find_engine( engine_world_mat_path( id ) );
		}

	}

	void scene::on_draw_scene_object_array( std::uintptr_t object_array ) const
	{
		const auto& scene_cfg = settings::g_world.m_scene;
		if ( !memory::is_game_ptr( object_array ) || !scene_cfg.world_setting.value )
			return;

		const auto object_data = memory::read<std::uintptr_t>( object_array + 0x8 );
		if ( !memory::is_game_ptr( object_data ) )
			return;

		if ( !addresses::globals::light_data_queue )
			return;

		const auto light_data_queue = memory::read<std::uintptr_t>( addresses::globals::light_data_queue );
		if ( !memory::is_game_ptr( light_data_queue ) )
			return;

		const auto light_data_base = memory::read<std::uintptr_t>( light_data_queue + 0x18 );
		const auto light_data_end = memory::read<std::uintptr_t>( light_data_queue + 0x8 );
		if ( !memory::is_game_ptr( light_data_base ) || !light_data_end || light_data_end <= light_data_base )
			return;

		const auto count = memory::read<int>( object_data + 0x4 );
		const auto index = memory::read<int>( object_data + 0x30 );
		if ( count <= 0 || index < 0 )
			return;

		const auto slots = ( light_data_end - light_data_base ) >> 5;
		if ( static_cast< std::uint64_t >( index ) >= slots )
			return;

		const auto remaining = slots - static_cast< std::uint64_t >( index );
		const auto write_count = static_cast< std::uint64_t >( count ) < remaining
			? count
			: static_cast< int >( remaining );

		const auto& configured = scene_cfg.world_color.value;
		for ( auto i = 0; i < write_count; ++i )
		{
			const auto color_addr = light_data_base +
				( ( static_cast< std::size_t >( index ) + i ) << 5 );
			const auto current = memory::read<xdraw::color>( color_addr );
			memory::write<xdraw::color>(
				color_addr, { configured.r, configured.g, configured.b, current.a } );
		}
	}

	void scene::on_draw_scene_object( std::uintptr_t batch, int batch_count ) const
	{
		if ( !memory::is_game_ptr( batch ) || batch_count <= 0 || batch_count > 8192 )
			return;

		const auto& scene_cfg = settings::g_world.m_scene;
		const auto& config = scene_cfg.skybox;
		const bool world_color_on  = scene_cfg.world_setting.value;
		const bool cloud_on = config.cloud_color.value.a != 0;
		const bool sun_on = config.sun_color.value.a != 0;
		if ( !world_color_on && !cloud_on && !sun_on )
			return;

		constexpr std::array cloud_materials{
			"materials/effects/clouds_001.vmat"_hash,
			"materials/de_vertigo/vertigo_clouds_001.vmat"_hash,
			"materials/models/props/de_nuke/hr_nuke/nuke_skydome_001/nuke_clouds_003.vmat"_hash,
			"materials/models/props/de_nuke/hr_nuke/nuke_skydome_001/nuke_clouds_002.vmat"_hash,
			"materials/models/props/de_nuke/hr_nuke/nuke_skydome_001/nuke_clouds_001.vmat"_hash
		};

		constexpr std::array sun_materials{
			"materials/sun/overlay.vmat"_hash,
			"materials/effects/glows/sun_glow_001.vmat"_hash,
			"materials/effects/glows/sun_disc_glow_001.vmat"_hash,
			"materials/effects/glows/sun_disc_glow_003.vmat"_hash,
			"materials/effects/glows/sun_disc_glow_004.vmat"_hash,
			"materials/de_train/hr_train_s2/effects/sun_disc_glow_01_clouded.vmat"_hash
		};

		const auto world_color_packed = world_color_on
			? *reinterpret_cast<const std::uint32_t*>( &scene_cfg.world_color.value ) : 0u;

		for ( auto i = 0; i < batch_count; ++i )
		{
			const auto entry = scene_data::at( batch, i );

			if ( !cloud_on && !sun_on )
			{
				if ( world_color_on )
					memory::write<std::uint32_t>( entry + scene_data::mesh_draw_primitive_t::color, world_color_packed );
				continue;
			}

			const auto material = memory::read<std::uintptr_t>( entry + scene_data::mesh_draw_primitive_t::material );
			if ( !material )
			{
				if ( world_color_on )
					memory::write<std::uint32_t>( entry + scene_data::mesh_draw_primitive_t::color, world_color_packed );
				continue;
			}

			const auto material_hash = detail::safe_material_hash( material );
			const auto is_cloud = material_hash && std::ranges::contains( cloud_materials, material_hash );
			const auto is_sun = !is_cloud && material_hash && std::ranges::contains( sun_materials, material_hash );

			if ( is_cloud && cloud_on )
			{
				memory::write<std::uint32_t>( entry + scene_data::mesh_draw_primitive_t::color, config.cloud_color.value );
			}
			else if ( is_sun && sun_on )
			{
				memory::write<std::uint32_t>( entry + scene_data::mesh_draw_primitive_t::color, config.sun_color.value );
			}
			else if ( world_color_on )
			{
				memory::write<std::uint32_t>( entry + scene_data::mesh_draw_primitive_t::color, world_color_packed );
			}
		}
	}

	bool scene::world_material_swap_active( ) noexcept
	{
		return settings::g_world.m_scene.world_material_swap.value;
	}

	std::uintptr_t scene::force_world_material_rcx( std::uintptr_t mesh_entry )
	{
		const auto& scene_cfg = settings::g_world.m_scene;
		if ( !mesh_entry || !scene_cfg.world_material_swap.value )
			return 0;

		const auto mat_id = scene_cfg.world_material.value;
		if ( !this->m_world_mat_slot
			|| this->m_world_mat_id != mat_id
			|| !memory::is_game_ptr( this->m_world_mat_slot ) )
		{
			this->m_world_mat_slot = find_world_replacement( mat_id );
			this->m_world_mat_id = mat_id;
		}

		if ( !this->m_world_mat_slot || !memory::is_game_ptr( this->m_world_mat_slot ) )
			return 0;

		const auto vtable = memory::safe_read<std::uintptr_t>( this->m_world_mat_slot );
		if ( !vtable || !memory::is_game_ptr( *vtable ) )
		{
			this->m_world_mat_slot = 0;
			return 0;
		}

		return reinterpret_cast< std::uintptr_t >( &this->m_world_mat_slot );
	}

	void scene::on_set_shader_param( __m128i*& value, std::uint32_t hash ) const
	{
		static __m128 wind_direction_val;
		static __m128 wind_strength_frequency_val;

		this->apply_worldblur_param( value, hash );

		const auto& weather = settings::g_world.m_weather;
		if ( weather.wind.value && hash == detail::shader_hash::wind_direction )
		{
			const auto direction = weather.wind_direction.value *
				( std::numbers::pi_v<float> / 180.0f );
			wind_direction_val = _mm_set_ps(
				0.0f, 0.0f, std::sin( direction ), std::cos( direction ) );
			value = reinterpret_cast< __m128i* >( &wind_direction_val );
		}
		else if ( weather.wind.value && hash == detail::shader_hash::wind_strength_frequency )
		{
			wind_strength_frequency_val = _mm_set_ps(
				weather.wind_turbulence.value,
				weather.wind_strength.value,
				weather.wind_turbulence.value,
				weather.wind_strength.value );
			value = reinterpret_cast< __m128i* >( &wind_strength_frequency_val );
		}

		this->apply_bloom_param( value, hash );
		this->apply_gamma_param( value, hash );
		this->apply_wetness_param( value, hash );
	}

}

