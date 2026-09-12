#pragma once

#include <core/common.hpp>
#include <core/memory.hpp>
#include <core/patterns.h>

namespace systems {

	namespace detail {
#include <modules/visuals/esp/materials/vmats_extra.h>
	}

	bool materials::initialize( )
	{
		using id = settings::esp::cham_ids;

		auto load_pair = [ & ]( id mat_id, const char* vis_data, const char* vis_name, const char* occ_data, const char* occ_name ) -> bool
		{
			const auto idx = static_cast< std::size_t >( mat_id );
			m_loaded[ idx ].visible = load( vis_data, vis_name );
			m_loaded[ idx ].occluded = load( occ_data, occ_name );
			return m_loaded[ idx ].visible && m_loaded[ idx ].occluded;
		};

		bool ok = true;
		ok &= load_pair( id::white, detail::white_vmat, xs( "materials/dev/white.vmat" ), detail::white_vmat_invis, xs( "materials/dev/white_invis.vmat" ) );
		ok &= load_pair( id::latex, detail::latex_vmat, xs( "materials/dev/latex.vmat" ), detail::latex_vmat_invis, xs( "materials/dev/latex_invis.vmat" ) );
		ok &= load_pair( id::glow, detail::glow_vmat, xs( "materials/dev/glow.vmat" ), detail::glow_vmat_invis, xs( "materials/dev/glow_invis.vmat" ) );
		ok &= load_pair( id::ghost, detail::ghost_vmat, xs( "materials/dev/ghost.vmat" ), detail::ghost_vmat_invis, xs( "materials/dev/ghost_invis.vmat" ) );
		ok &= load_pair( id::flat, detail::flat_vmat, xs( "materials/dev/flat.vmat" ), detail::flat_vmat_invis, xs( "materials/dev/flat_invis.vmat" ) );
		ok &= load_pair( id::glow2, detail::bloom2_vmat, xs( "materials/dev/glow2.vmat" ), detail::bloom2_vmat_invis, xs( "materials/dev/glow2_invis.vmat" ) );
		ok &= load_pair( id::glass, detail::glass_vmat, xs( "materials/dev/glass.vmat" ), detail::glass_vmat_invis, xs( "materials/dev/glass_invis.vmat" ) );
		ok &= load_pair( id::generic, detail::generic_vmat, xs( "materials/dev/generic.vmat" ), detail::generic_vmat_invis, xs( "materials/dev/generic_invis.vmat" ) );
		ok &= load_pair( id::unlit, detail::unlit_vmat, xs( "materials/dev/unlit.vmat" ), detail::unlit_vmat_invis, xs( "materials/dev/unlit_invis.vmat" ) );
		ok &= load_pair( id::solid, detail::solid_vmat, xs( "materials/dev/solid.vmat" ), detail::solid_vmat_invis, xs( "materials/dev/solid_invis.vmat" ) );
		ok &= load_pair( id::wireframe, detail::wireframe_vmat, xs( "materials/dev/wireframe.vmat" ), detail::wireframe_vmat_invis, xs( "materials/dev/wireframe_invis.vmat" ) );
		ok &= load_pair( id::bloom, detail::bloom_vmat, xs( "materials/dev/bloom.vmat" ), detail::bloom_vmat_invis, xs( "materials/dev/bloom_invis.vmat" ) );
		ok &= load_pair( id::illuminate, detail::illuminate_vmat, xs( "materials/dev/illuminate.vmat" ), detail::illuminate_vmat_invis, xs( "materials/dev/illuminate_invis.vmat" ) );
		ok &= load_pair( id::gost, detail::gost_vmat, xs( "materials/dev/gost.vmat" ), detail::gost_vmat_invis, xs( "materials/dev/gost_invis.vmat" ) );
		ok &= load_pair( id::crystal, detail::crystal_vmat, xs( "materials/dev/crystal.vmat" ), detail::crystal_vmat_invis, xs( "materials/dev/crystal_invis.vmat" ) );
		ok &= load_pair( id::gost2, detail::gost2_vmat, xs( "materials/dev/gost2.vmat" ), detail::gost2_vmat_invis, xs( "materials/dev/gost2_invis.vmat" ) );
		ok &= load_pair( id::metallic, detail::metallic_vmat, xs( "materials/dev/metallic.vmat" ), detail::metallic_vmat_invis, xs( "materials/dev/metallic_invis.vmat" ) );
		ok &= load_pair( id::flow, detail::flow_vmat, xs( "materials/dev/flow.vmat" ), detail::flow_vmat_invis, xs( "materials/dev/flow_invis.vmat" ) );
		ok &= load_pair( id::darkmatter, detail::darkmatter_vmat, xs( "materials/dev/darkmatter.vmat" ), detail::darkmatter_vmat_invis, xs( "materials/dev/darkmatter_invis.vmat" ) );
		ok &= load_pair( id::data, detail::data_vmat, xs( "materials/dev/data.vmat" ), detail::data_vmat_invis, xs( "materials/dev/data_invis.vmat" ) );
		ok &= load_pair( id::chrome, detail::chrome_vmat, xs( "materials/dev/chrome.vmat" ), detail::chrome_vmat_invis, xs( "materials/dev/chrome_invis.vmat" ) );
		ok &= load_pair( id::plastic, detail::plastic_vmat, xs( "materials/dev/plastic.vmat" ), detail::plastic_vmat_invis, xs( "materials/dev/plastic_invis.vmat" ) );
		ok &= load_pair( id::energy, detail::energy_vmat, xs( "materials/dev/energy.vmat" ), detail::energy_vmat_invis, xs( "materials/dev/energy_invis.vmat" ) );
		ok &= load_pair( id::hologram, detail::hologram_vmat, xs( "materials/dev/hologram.vmat" ), detail::hologram_vmat_invis, xs( "materials/dev/hologram_invis.vmat" ) );
		ok &= load_pair( id::galaxy, detail::galaxy_vmat, xs( "materials/dev/galaxy.vmat" ), detail::galaxy_vmat_invis, xs( "materials/dev/galaxy_invis.vmat" ) );
		ok &= load_pair( id::gold, detail::gold_vmat, xs( "materials/dev/gold.vmat" ), detail::gold_vmat_invis, xs( "materials/dev/gold_invis.vmat" ) );
		ok &= load_pair( id::neon, detail::neon_vmat, xs( "materials/dev/neon.vmat" ), detail::neon_vmat_invis, xs( "materials/dev/neon_invis.vmat" ) );
		ok &= load_pair( id::xray, detail::xray_vmat, xs( "materials/dev/xray.vmat" ), detail::xray_vmat_invis, xs( "materials/dev/xray_invis.vmat" ) );
		ok &= load_pair( id::liquid, detail::liquid_vmat, xs( "materials/dev/liquid.vmat" ), detail::liquid_vmat_invis, xs( "materials/dev/liquid_invis.vmat" ) );
		ok &= load_pair( id::pearl, detail::pearl_vmat, xs( "materials/dev/pearl.vmat" ), detail::pearl_vmat_invis, xs( "materials/dev/pearl_invis.vmat" ) );
		ok &= load_pair( id::distortion, detail::distortion_vmat, xs( "materials/dev/distortion.vmat" ), detail::distortion_vmat_invis, xs( "materials/dev/distortion_invis.vmat" ) );
		ok &= load_pair( id::outlines, detail::outlines_vmat, xs( "materials/dev/outlines.vmat" ), detail::outlines_vmat_invis, xs( "materials/dev/outlines_invis.vmat" ) );

		return ok;
	}

	std::uintptr_t materials::find( settings::esp::cham_ids id, bool occluded )
	{
		const auto index = static_cast< std::size_t >( id );
		if ( index >= m_loaded.size( ) )
		{
			return 0;
		}

		return occluded ? m_loaded[ index ].occluded : m_loaded[ index ].visible;
	}

	namespace engine_mat_detail {

		inline std::unordered_map<std::uint32_t, std::uintptr_t> engine_cache{};
		inline std::mutex engine_mtx{};

		[[nodiscard]] inline const char* material_name( std::uintptr_t mat ) noexcept
		{
			if ( !memory::is_game_ptr( mat ) )
				return nullptr;

			return memory::call_vfunc<const char*>( mat, 0 );
		}

		[[nodiscard]] inline std::uintptr_t resolve_material_ptr( std::uintptr_t candidate ) noexcept
		{
			if ( !candidate || !memory::is_game_ptr( candidate ) )
			{
				return 0;
			}

			if ( material_name( candidate ) )
			{
				return candidate;
			}

			const auto inner = memory::read<std::uintptr_t>( candidate );
			if ( inner && material_name( inner ) )
			{
				return inner;
			}

			return 0;
		}

		struct matsys_probe_result
		{
			std::uintptr_t result{};
			std::uintptr_t out{};
		};

		[[nodiscard]] inline matsys_probe_result call_matsys_index(
			std::uintptr_t material_system,
			std::size_t index,
			const char* path ) noexcept
		{
			matsys_probe_result probe{};
			probe.result = memory::call_vfunc<std::uintptr_t>(
				material_system, index, &probe.out, path );
			return probe;
		}

		struct resource_probe_result
		{
			std::uintptr_t binding{};
			std::uintptr_t from_binding{};
			std::uintptr_t material{};
		};

		[[nodiscard]] inline resource_probe_result load_vmat_resource( const char* path ) noexcept
		{
			resource_probe_result probe{};

			struct buffer_string
			{
				std::uint32_t m_unknown1{};
				std::uint32_t m_unknown2{ 0xc00000c8 };

				union
				{
					std::uintptr_t m_str_ptr;
					std::uint8_t data[ 0xc8 ];
				};

				std::uintptr_t m_unknown3{};
				std::uintptr_t m_unknown4{};
			} buffer{};

			const auto init_path_buffer = PATTERN( PATTERN_INIT_PARTICLE_PATH_BUFFER );
			const auto precache_resource = PATTERN( PATTERN_RESOURCE_SYSTEM_PRECACHE );
			if ( !init_path_buffer || !precache_resource || !addresses::globals::resource_system )
			{
				return probe;
			}

			memory::call<void>( init_path_buffer, &buffer, path );
			buffer.m_unknown4 = 'tamv';
			memory::call<void>( precache_resource, addresses::globals::resource_system, &buffer, "" );

			memory::call<void>( init_path_buffer, &buffer, path );
			buffer.m_unknown4 = 'tamv';

			probe.binding = memory::call_vfunc<std::uintptr_t>(
				addresses::globals::resource_system, 79, &buffer, 0ll );
			probe.from_binding = probe.binding ? memory::read<std::uintptr_t>( probe.binding ) : 0;
			probe.material = resolve_material_ptr(
				probe.from_binding ? probe.from_binding : probe.binding );

			return probe;
		}

	}

	std::uintptr_t materials::find_engine( const char* path )
	{
		if ( !path || !*path )
		{
			return 0;
		}

		const auto key = fnv1a::runtime_hash( path );
		{
			std::scoped_lock lock( engine_mat_detail::engine_mtx );
			const auto it = engine_mat_detail::engine_cache.find( key );
			if ( it != engine_mat_detail::engine_cache.end( ) )
			{
				if ( memory::is_game_ptr( it->second ) )
				{
					return it->second;
				}

				engine_mat_detail::engine_cache.erase( it );
			}
		}

		std::uintptr_t material{};

		{
			const auto probe = engine_mat_detail::load_vmat_resource( path );
			material = probe.material;
		}

		if ( !material && addresses::globals::material_system )
		{
			static constexpr std::size_t k_indices[]{ 14, 15, 16, 20, 21, 29 };
			for ( const auto index : k_indices )
			{
				const auto probe = engine_mat_detail::call_matsys_index(
					addresses::globals::material_system, index, path );

				const auto candidate_a = engine_mat_detail::resolve_material_ptr( probe.result );
				const auto candidate_b = engine_mat_detail::resolve_material_ptr( probe.out );

				material = candidate_a ? candidate_a : candidate_b;
				if ( material )
				{
					break;
				}
			}
		}

		if ( material && !memory::is_game_ptr( material ) )
		{
			material = 0;
		}

		if ( !material )
		{
			return 0;
		}

		{
			std::scoped_lock lock( engine_mat_detail::engine_mtx );
			engine_mat_detail::engine_cache.emplace( key, material );
		}

		return material;
	}

	const char* materials::get_texture_path( std::uintptr_t entry )
	{
		const auto handle = *reinterpret_cast< const std::uintptr_t* >( entry + 0x10 );
		if ( !handle )
		{
			return nullptr;
		}

		const auto resource = *reinterpret_cast< const std::uintptr_t* >( handle + 0x08 );
		if ( !resource )
		{
			return nullptr;
		}

		return *reinterpret_cast< const char** >( resource );
	}

	std::string materials::emit_translucent_kv( std::uintptr_t src_mat )
	{
		if ( !memory::is_game_ptr( src_mat ) )
		{
			return {};
		}

		const auto kv_count_opt = memory::safe_read<int>( src_mat + 0x18 );
		const auto kv_array_opt = memory::safe_read<std::uintptr_t>( src_mat + 0x20 );
		if ( !kv_count_opt || !kv_array_opt || !memory::is_game_ptr( *kv_array_opt ) )
		{
			return {};
		}

		const auto kv_count = std::clamp( *kv_count_opt, 0, 4096 );
		const auto kv_array = *kv_array_opt;

		std::string kv =
			"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} "
			"format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
			"{\n"
			"shader = \"csgo_complex.vfx\"\n"
			"F_TRANSLUCENT = 1\n"
			"F_ALPHA_TEST = 0\n"
			"F_ADDITIVE_BLEND = 0\n";

		auto should_skip = [ ]( const char* name ) -> bool
			{
			if ( std::strcmp( name, "shader" ) == 0 )
				{
					return true;
				}

			if ( std::strncmp( name, "F_", 2 ) == 0 )
				{
					return true;
				}

				return false;
			};

		for ( auto i = 0; i < kv_count; ++i )
		{
			const auto entry = kv_array + static_cast< std::uintptr_t >( i ) * 0x40;
			const auto name = *reinterpret_cast< const char** >( entry + 0x28 );

			if ( !name || should_skip( name ) )
			{
				continue;
			}

			if ( *reinterpret_cast< const std::uintptr_t* >( entry + 0x10 ) )
			{
				const auto path = get_texture_path( entry );
				if ( path && path[ 0 ] )
				{
					kv += std::format( "{} = resource:\"{}\"\n", name, path );
				}

				continue;
			}

		if ( std::strncmp( name, "g_b", 3 ) == 0 )
			{
				const auto v = static_cast< int >( *reinterpret_cast< const float* >( entry ) );
				kv += std::format( "{} = {}\n", name, v ? 1 : 0 );
				continue;
			}

		if ( std::strncmp( name, "g_n", 3 ) == 0 )
			{
				const auto v = static_cast< int >( *reinterpret_cast< const float* >( entry ) );
				kv += std::format( "{} = {}\n", name, v );
				continue;
			}

		if ( std::strncmp( name, "g_fl", 4 ) == 0 || std::strncmp( name, "g_f", 3 ) == 0 )
			{
				const auto v = *reinterpret_cast< const float* >( entry );
				kv += std::format( "{} = {}\n", name, v );
				continue;
			}

		if ( std::strncmp( name, "g_v", 3 ) == 0 )
			{
				const auto x = *reinterpret_cast< const float* >( entry );
				const auto y = *reinterpret_cast< const float* >( entry + 0x04 );
				const auto z = *reinterpret_cast< const float* >( entry + 0x08 );
				kv += std::format( "{} = [{}, {}, {}]\n", name, x, y, z );
				continue;
			}
		}

		kv += "}\n";
		return kv;
	}

	std::string materials::emit_ignorez_kv( std::uintptr_t src_mat )
	{
		if ( !memory::is_game_ptr( src_mat ) )
		{
			return {};
		}

		const auto kv_count_opt = memory::safe_read<int>( src_mat + 0x18 );
		const auto kv_array_opt = memory::safe_read<std::uintptr_t>( src_mat + 0x20 );
		if ( !kv_count_opt || !kv_array_opt || !memory::is_game_ptr( *kv_array_opt ) )
		{
			return {};
		}

		const auto kv_count = std::clamp( *kv_count_opt, 0, 4096 );
		const auto kv_array = *kv_array_opt;

		std::string kv =
			"<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} "
			"format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->\n"
			"{\n"
			"shader = \"csgo_complex.vfx\"\n"
			"F_TRANSLUCENT = 1\n"
			"F_DISABLE_Z_BUFFERING = 1\n"
			"F_ALPHA_TEST = 0\n"
			"F_ADDITIVE_BLEND = 0\n";

		auto should_skip = [ ]( const char* name ) -> bool
			{
			if ( std::strcmp( name, "shader" ) == 0 )
				{
					return true;
				}

			if ( std::strncmp( name, "F_", 2 ) == 0 )
				{
					return true;
				}

				return false;
			};

		for ( auto i = 0; i < kv_count; ++i )
		{
			const auto entry = kv_array + static_cast< std::uintptr_t >( i ) * 0x40;
			const auto name = *reinterpret_cast< const char** >( entry + 0x28 );

			if ( !name || should_skip( name ) )
			{
				continue;
			}

			if ( *reinterpret_cast< const std::uintptr_t* >( entry + 0x10 ) )
			{
				const auto path = get_texture_path( entry );
				if ( path && path[ 0 ] )
				{
					kv += std::format( "{} = resource:\"{}\"\n", name, path );
				}

				continue;
			}

		if ( std::strncmp( name, "g_b", 3 ) == 0 )
			{
				const auto v = static_cast< int >( *reinterpret_cast< const float* >( entry ) );
				kv += std::format( "{} = {}\n", name, v ? 1 : 0 );
				continue;
			}

		if ( std::strncmp( name, "g_n", 3 ) == 0 )
			{
				const auto v = static_cast< int >( *reinterpret_cast< const float* >( entry ) );
				kv += std::format( "{} = {}\n", name, v );
				continue;
			}

		if ( std::strncmp( name, "g_fl", 4 ) == 0 || std::strncmp( name, "g_f", 3 ) == 0 )
			{
				const auto v = *reinterpret_cast< const float* >( entry );
				kv += std::format( "{} = {}\n", name, v );
				continue;
			}

		if ( std::strncmp( name, "g_v", 3 ) == 0 )
			{
				const auto x = *reinterpret_cast< const float* >( entry );
				const auto y = *reinterpret_cast< const float* >( entry + 0x04 );
				const auto z = *reinterpret_cast< const float* >( entry + 0x08 );
				kv += std::format( "{} = [{}, {}, {}]\n", name, x, y, z );
				continue;
			}
		}

		kv += "}\n";
		return kv;
	}

	std::uintptr_t materials::load( const char* vmat_data, const char* name )
	{
		constexpr auto kv3_id = cstypes::kv3_id{ "generic", 0x41B818518343427E, 0xB5F447C23C0CDF8C };

		if ( !vmat_data || !name )
		{
			return 0;
		}

		const auto kv3_set_type = PATTERN(PATTERN_KV3_ALLOC);
		const auto kv3_destroy = PATTERN(PATTERN_KV3_DESTROY);
		const auto kv3_load = MODULE_EXPORT( "tier0.dll:?LoadKV3@@YA_NPEAVKeyValues3@@PEAVCUtlString@@PEBDAEBUKV3ID_t@@2I@Z" );
		const auto material_create = PATTERN(PATTERN_MATERIAL_CREATE);
		if ( !kv3_set_type || !kv3_destroy || !kv3_load || !material_create )
		{
			return 0;
		}

		cstypes::key_values3 kv3{};
		if ( memory::call<cstypes::key_values3*>( kv3_set_type, &kv3, 1u, 6u ) != &kv3 )
		{
			return 0;
		}

		cstypes::strong_handle handle{};
		const auto loaded = memory::call<bool>( kv3_load, &kv3, nullptr, vmat_data, &kv3_id, nullptr, 0u );
		if ( loaded )
		{
			memory::call<void*>( material_create, nullptr, &handle, name, &kv3, 0, true );
		}

		memory::call<void>( kv3_destroy, &kv3, 0u );

		if ( !loaded || !handle.binding )
		{
			return 0;
		}

		const auto material = *reinterpret_cast< const std::uintptr_t* >( handle.binding );
		if ( material )
		{
			std::scoped_lock lock( m_mtx );
			m_handles.push_back( handle );
		}

		return material;
	}

	std::uintptr_t materials::get_or_create_clone( std::uintptr_t src_mat, clone_type type )
	{
		if ( !memory::is_game_ptr( src_mat ) )
		{
			return 0;
		}

		const auto key = src_mat ^ ( static_cast< std::uint64_t >( type ) << 48 );

		{
			std::scoped_lock lock( m_mtx );
			const auto it = m_map.find( key );
			if ( it != m_map.end( ) )
			{
				return it->second;
			}
		}

		std::scoped_lock create_lock( m_create_mtx );

		{
			std::scoped_lock lock( m_mtx );
			const auto it = m_map.find( key );
			if ( it != m_map.end( ) )
			{
				return it->second;
			}
		}

		const auto kv = type == clone_type::ignorez ? emit_ignorez_kv( src_mat ) : emit_translucent_kv( src_mat );
		if ( kv.empty( ) )
		{
			return 0;
		}

		const auto prefix = type == clone_type::ignorez ? "_ignorez" : "_clone";
		const auto name = std::format( "materials/{}_{:x}.vmat", prefix, src_mat );
		const auto mat = load( kv.c_str( ), name.c_str( ) );
		if ( !mat )
		{
			return 0;
		}

		{
			std::scoped_lock lock( m_mtx );
			m_map.emplace( key, mat );
		}

		return mat;
	}

	void materials::clear_clones( )
	{
		std::scoped_lock lock( m_mtx );
		m_map.clear( );
	}

	void materials::clear_engine_cache( )
	{
		std::scoped_lock lock( engine_mat_detail::engine_mtx );
		engine_mat_detail::engine_cache.clear( );
	}

	void materials::set_material_vec3( std::uintptr_t mat, const char* param_name, float x, float y, float z )
	{
		const auto kv_count = *reinterpret_cast< const int* >( mat + 0x18 );
		const auto kv_array = *reinterpret_cast< const std::uintptr_t* >( mat + 0x20 );

		for ( auto i = 0; i < kv_count; ++i )
		{
			const auto entry = kv_array + static_cast< std::uintptr_t >( i ) * 0x40;
			const auto name = *reinterpret_cast< const char** >( entry + 0x28 );

		if ( !name || std::strcmp( name, param_name ) != 0 )
			{
				continue;
			}

			*reinterpret_cast< float* >( entry + 0x00 ) = x;
			*reinterpret_cast< float* >( entry + 0x04 ) = y;
			*reinterpret_cast< float* >( entry + 0x08 ) = z;
			return;
		}
	}

}





