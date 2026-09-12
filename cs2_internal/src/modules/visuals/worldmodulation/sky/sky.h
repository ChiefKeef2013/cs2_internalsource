#pragma once

#include <modules/visuals/worldmodulation/world.hpp>
#include <core/patterns.h>
#include <valve/schemas/CCSGameRules.h>

namespace features::world {

	void scene::discover_skyboxes () {
		this->m_skyboxes.clear ();
		this->m_skyboxes.reserve (detail::skybox_paths.size () + 16);
		for (auto index = std::size_t {}; index < detail::skybox_paths.size (); ++index) {
			this->m_skyboxes.push_back ({detail::skybox_names [index], detail::skybox_paths [index]});
		}

		const auto directory = detail::find_skybox_directory ();
		if (directory.empty ()) {
			return;
		}

		std::vector<skybox_entry> custom_skyboxes;
		std::error_code error;
	for (const auto& file : std::filesystem::directory_iterator (
			directory, std::filesystem::directory_options::skip_permission_denied, error)) {
			if (error) {
				break;
			}
			if (!file.is_regular_file (error)) {
				continue;
			}

			const auto filename = file.path ().filename ().string ();
			if (filename.size () < 8 || !filename.ends_with (".vtex_c")) {
				continue;
			}

			const auto base = filename.substr (0, filename.size () - 7);
			auto lower = base;
		std::ranges::transform (lower, lower.begin (), [] (const unsigned char character) {
				return static_cast<char> (std::tolower (character));
			});
			if (lower.contains ("_fog") || lower.contains ("_lighting") || lower.contains ("_v1")) {
				continue;
			}

			auto resource_path = std::string {"materials/skybox/"} + base + ".vtex";
			const auto duplicate = [&resource_path] (const skybox_entry& entry) {
				return entry.resource_path == resource_path;
			};
			if (std::ranges::any_of (this->m_skyboxes, duplicate) ||
			std::ranges::any_of (custom_skyboxes, duplicate)) {
				continue;
			}

			custom_skyboxes.push_back ({detail::prettify_skybox_name (base), std::move (resource_path)});
		}

		std::ranges::sort (custom_skyboxes, {}, &skybox_entry::display_name);
		this->m_skyboxes.insert (
			this->m_skyboxes.end (),
			std::make_move_iterator (custom_skyboxes.begin ()),
			std::make_move_iterator (custom_skyboxes.end ()));
	}

	void scene::clear_active_skybox( )
	{
		this->m_active_material_binding = 0;
		this->m_active_original_material = 0;
		this->m_active_skybox_descriptor = 0;
		this->m_active_sky_tinted = false;
	}

	void scene::reset_skybox_state( )
	{
		this->clear_active_skybox( );
		this->m_custom_sky_material = 0;
		this->m_loaded_skybox_index = -1;
	}

	void scene::invalidate_runtime( bool drop_material_cache )
	{
		this->clear_active_skybox( );
		this->m_world_mat_slot = 0;
		this->m_scene_settle_until = std::chrono::duration_cast< std::chrono::milliseconds >(
			std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( ) + 1500;
		if ( drop_material_cache )
		{
			this->m_custom_sky_material = 0;
			this->m_loaded_skybox_index = -1;
			this->m_skybox_materials.clear( );
			systems::materials::clear_engine_cache( );
		}
	}

	void scene::on_round_start( )
	{
		this->invalidate_runtime( false );
	}

	void scene::on_frame_stage_notify( )
	{
		const auto game_rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
		if ( game_rules )
		{
			const auto round_start_time = reinterpret_cast< C_CSGameRules* >( game_rules )->m_fRoundStartTime( );
			if ( round_start_time != this->m_last_round_start_time )
			{
				this->m_last_round_start_time = round_start_time;
				this->on_round_start( );
			}
		}

		const auto local = systems::g_local.get( );
		if ( !local.pawn || !local.is_alive )
		{
			this->reset_skybox_state( );
			return;
		}

		const auto& config = settings::g_world.m_scene.skybox;
		if ( !config.custom_skybox || this->m_skyboxes.empty( ) )
			return;

		const auto idx = std::clamp( config.selected_skybox.value, 0, static_cast< int >( this->m_skyboxes.size( ) ) - 1 );
		if ( idx != this->m_loaded_skybox_index )
		{
			this->load_skybox_material( this->m_skyboxes[ idx ].resource_path.c_str( ) );
			this->m_loaded_skybox_index = idx;
		}
	}

	void scene::on_draw_skybox_array_pre( std::uintptr_t mesh_array, int mesh_count )
	{
		this->clear_active_skybox( );

		if ( !memory::is_game_ptr( mesh_array ) || mesh_count <= 0 || !systems::g_local.get( ).pawn )
			return;

		const auto& config = settings::g_world.m_scene.skybox;
		const auto replace_material = config.custom_skybox.value && this->m_custom_sky_material;
		if ( !replace_material && !config.custom_color.value )
			return;

		const auto skybox_object = memory::read<std::uintptr_t>(
			mesh_array + ( static_cast< std::size_t >( mesh_count ) * 0x70 ) - 0x58 );
		if ( !memory::is_game_ptr( skybox_object ) )
			return;

		this->m_active_skybox_descriptor = skybox_object;

		if ( replace_material )
		{
			const auto material_binding = memory::read<std::uintptr_t>( skybox_object + 0xD0 );
			if ( memory::is_game_ptr( material_binding ) )
			{
				const auto original_material = memory::read<std::uintptr_t>( material_binding );
				if ( memory::is_game_ptr( original_material ) )
				{
					memory::write<std::uintptr_t>( material_binding, this->m_custom_sky_material );
					this->m_active_material_binding = material_binding;
					this->m_active_original_material = original_material;
				}
			}
		}

		if ( config.custom_color.value )
		{
			const auto original_color = memory::read<std::array<float, 3>>( skybox_object + 0xE8 );
			const auto color = config.skybox_color.value.to_float( );
			const std::array<float, 3> replacement{ color[ 0 ], color[ 1 ], color[ 2 ] };
			memory::write( skybox_object + 0xE8, replacement );
			this->m_active_original_sky_color = original_color;
			this->m_active_sky_tinted = true;
		}
	}

	void scene::on_draw_skybox_array_post( )
	{
		if ( this->m_active_material_binding && this->m_active_original_material )
		{
			memory::write<std::uintptr_t>(
				this->m_active_material_binding, this->m_active_original_material );
		}

		if ( this->m_active_sky_tinted && this->m_active_skybox_descriptor )
		{
			memory::write(
				this->m_active_skybox_descriptor + 0xE8, this->m_active_original_sky_color );
		}

		this->clear_active_skybox( );
	}

	void scene::load_skybox_material( const char* path )
	{
		this->m_custom_sky_material = 0;
		if ( !path || !*path )
			return;

		if ( const auto cached = this->m_skybox_materials.find( path );
			cached != this->m_skybox_materials.end( ) )
		{
			this->m_custom_sky_material = cached->second.material;
			return;
		}

		if (!detail::is_safe_sky_texture_path (path)) {
			return;
		}

		struct buffer_string {
			std::uint32_t m_unknown1 {};
			std::uint32_t m_unknown2 {0xc00000c8};

			union {
				std::uintptr_t m_str_ptr;
				std::uint8_t data [0xc8];
			};

			std::uintptr_t m_unknown3 {};
			std::uintptr_t m_unknown4 {};
		} buffer;

		const auto init_path_buffer = PATTERN(PATTERN_INIT_PARTICLE_PATH_BUFFER);
		const auto precache_resource = PATTERN(PATTERN_RESOURCE_SYSTEM_PRECACHE);
		if (!init_path_buffer || !precache_resource || !addresses::globals::resource_system) {
			return;
		}

		memory::call<void> (init_path_buffer, &buffer, path);
		buffer.m_unknown4 = 'xetv';

		memory::call<void> (precache_resource, addresses::globals::resource_system, &buffer, "");

		memory::call<void> (init_path_buffer, &buffer, path);
		buffer.m_unknown4 = 'xetv';

		const auto binding = memory::call_vfunc<std::uintptr_t> (addresses::globals::resource_system, 79, &buffer, 0ll);
		if (!binding) {
			return;
		}
		const auto texture = memory::read<std::uintptr_t> (binding);
		if (!texture) {
			return;
		}

		const auto material_source = std::format (R"VMAT(<!-- kv3 encoding:text:version{{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d}} format:generic:version{{7412167c-06e9-4698-aff2-e63eb59037e7}} -->
{{
	Shader = "sky.vfx"
	g_flBrightnessExposureBias = 0.0
	g_flRenderOnlyExposureBias = 0.0
	SkyTexture = resource:"{}"
	g_tSkyTexture = resource:"{}"
}}
)VMAT", path, path);
		const auto material_name = std::format(
			"cs2_internal_skybox_{:08x}", fnv1a::runtime_hash( path ) );
		const auto material = systems::materials::load (material_source.c_str (), material_name.c_str ());
		if (!material) {
			return;
		}

		this->m_skybox_materials.emplace (path, cached_skybox_material {binding, material});
		this->m_custom_sky_material = material;
	}

}
