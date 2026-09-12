#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>

#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace features::world {

	namespace detail {

		inline constexpr std::array skybox_names{
			"Map default",
			"Vertigo",
			"Mirage",
			"Dust 2",
			"Nuke",
			"Anubis",
			"Overpass",
			"Train",
			"Aztec",
			"Italy",
			"Office",
			"Cloudy",
			"Rain Night",
			"Daylight",
			"Jungle",
			"Sunset",
			"Overcast",
			"Night",
			"Moon",
			"Hell",
			"The space",
			"Stars",
			"Aurora",
			"Space gas",
			"Space cells",
			"Stones"
		};

		inline constexpr std::array skybox_paths{
			"",
			"materials/skybox/sky_de_vertigo_exr_c70a3937.vtex",
			"materials/skybox/sky_de_mirage_exr_71e5f2a1.vtex",
			"materials/skybox/sky_de_dust2_exr_908a35ba.vtex",
			"materials/skybox/sky_de_nuke_exr_f04e84b2.vtex",
			"materials/skybox/sky_de_annubis_exr_2c5e0b53.vtex",
			"materials/skybox/sky_de_overpass_01_exr_f8534391.vtex",
			"materials/skybox/sky_de_train03_exr_4fdb8a38.vtex",
			"materials/skybox/sky_hr_aztec_02_exr_f84f8de9.vtex",
			"materials/skybox/cs_italy_s2_skybox_sunset_2_exr_e56cedf6.vtex",
			"materials/skybox/sky_cs_office_45_0_exr_d0152542.vtex",
			"materials/skybox/sky_csgo_cloudy01_cube_pfm_f9a0b177.vtex",
			"materials/skybox/sky_rain_night_01_exr_8d775aee.vtex",
			"materials/skybox/sky_cs15_daylight01_hdr_cube_pfm_a4b050d1.vtex",
			"materials/skybox/jungle_cube_pfm_bc16d813.vtex",
			"materials/skybox/tests/src/lightingtest_sky_sunset_light_exr_f7b19a45.vtex",
			"materials/skybox/sky_overcast_01_exr_da4019b1.vtex",
			"materials/skybox/tests/src/lightingtest_sky_night_exr_2c5e8c62.vtex",
			"materials/skybox/mr_moon_cube_pfm_f3262f9.vtex",
			"materials/skybox/hell_hdri_png_795ff36e.vtex",
			"materials/skybox/mr_21_cube_pfm_ea3e7de9.vtex",
			"materials/skybox/starmap_random_2020_4k_exr_5cc2c022.vtex",
			"materials/skybox/auroraborealis_cube_pfm_84850c18.vtex",
			"materials/skybox/space_skybox_jpg_6e5f57ce.vtex",
			"materials/skybox/dzy5_cube_pfm_ef2ee53f.vtex",
			"materials/skybox/space_13_cube_pfm_766b5180.vtex"
		};

		static_assert( skybox_names.size( ) == skybox_paths.size( ) );

		namespace shader_hash {
			constexpr std::uint32_t wind_direction{ 0x2A416C12 };
			constexpr std::uint32_t wind_strength_frequency{ 0xEB0D997E };
			constexpr std::uint32_t rain_exposure_to_sky{ 0x374C1B3C };
			constexpr std::uint32_t rain_timer{ 0x2DBEE393 };
			constexpr std::uint32_t rain_wetness{ 0x0F592812 };
			constexpr std::uint32_t gradient_fog{ 0x4B01FF63 };
			constexpr std::uint32_t gradient_fog_2{ 0x0AA49C2A };
			constexpr std::uint32_t gradient_fog_3{ 0xFBF6448D };
			constexpr std::uint32_t enable_gradient_fog{ 0x6E0FAD7E };
		}

		[[nodiscard]] inline std::uint32_t safe_material_hash( std::uintptr_t material ) noexcept
		{
			if ( !memory::is_game_ptr( material ) )
				return 0;

			thread_local std::uintptr_t last_material{};
			thread_local std::uint32_t last_hash{};
			if ( material == last_material )
				return last_hash;

			const auto name = memory::call_vfunc<const char*>( material, 0 );
			const auto hash = name && memory::detail::is_user_addr( reinterpret_cast< std::uintptr_t >( name ) )
				? fnv1a::runtime_hash( name )
				: 0;
			last_material = material;
			last_hash = hash;
			return hash;
		}

		[[nodiscard]] inline std::filesystem::path find_skybox_directory( )
		{
			std::array<wchar_t, 32768> module_path{};
			const auto length = GetModuleFileNameW(
				nullptr, module_path.data( ), static_cast< DWORD >( module_path.size( ) ) );
			if ( length && length < module_path.size( ) )
			{
				std::filesystem::path current{ module_path.data( ), module_path.data( ) + length };
				std::error_code error;

				{
					const auto candidate = current.parent_path( ).parent_path( ).parent_path( )
						/ L"csgo" / L"materials" / L"skybox";
					if ( std::filesystem::is_directory( candidate, error ) )
						return candidate;
					error.clear( );
				}

				for ( auto depth = 0; depth < 8 && current.has_parent_path( ); ++depth )
				{
					current = current.parent_path( );

					auto candidate = current / L"csgo" / L"materials" / L"skybox";
					if ( std::filesystem::is_directory( candidate, error ) )
						return candidate;
					error.clear( );

					candidate = current / L"game" / L"csgo" / L"materials" / L"skybox";
					if ( std::filesystem::is_directory( candidate, error ) )
						return candidate;
					error.clear( );
				}
			}

			static constexpr const wchar_t* fallbacks[]{
				L"D:\\SteamLibrary\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\materials\\skybox",
				L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Counter-Strike Global Offensive\\game\\csgo\\materials\\skybox"
			};

			for ( const auto* fallback : fallbacks )
			{
				std::error_code error;
				const std::filesystem::path candidate{ fallback };
				if ( std::filesystem::is_directory( candidate, error ) )
					return candidate;
			}

			return {};
		}

		[[nodiscard]] inline std::string prettify_skybox_name( std::string name )
		{
			constexpr std::array prefixes{
				"sky_de_", "sky_hr_", "sky_cs15_", "sky_cs_", "sky_csgo_", "sky_", "cs_"
			};
			for ( const auto* prefix : prefixes )
			{
				const auto length = std::strlen( prefix );
				if ( name.size( ) > length && name.compare( 0, length, prefix ) == 0 )
				{
					name.erase( 0, length );
					break;
				}
			}

			auto capitalize = true;
			for ( auto& character : name )
			{
				if ( character == '_' )
				{
					character = ' ';
					capitalize = true;
					continue;
				}

				const auto value = static_cast<unsigned char>( character );
				character = static_cast<char>( capitalize ? std::toupper( value ) : std::tolower( value ) );
				capitalize = false;
			}

			return name;
		}

		[[nodiscard]] inline bool is_safe_sky_texture_path( std::string_view path )
		{
			if ( path.empty( ) || path.size( ) > 512 || !path.ends_with( ".vtex" ) )
				return false;

			return std::ranges::none_of( path, []( const unsigned char character )
				{
					return character < 0x20 || character == '"' || character == '\'';
				} );
		}

	}

	namespace scene_data {

		struct mesh_draw_primitive_t
		{
			static constexpr std::size_t stride{ 0x70 };
			static constexpr std::ptrdiff_t scene_object{ 0x18 };
			static constexpr std::ptrdiff_t material{ 0x20 };
			static constexpr std::ptrdiff_t material2{ 0x28 };
			static constexpr std::ptrdiff_t color{ 0x50 };
			static constexpr std::ptrdiff_t color_extra{ 0x54 };
			static constexpr std::ptrdiff_t flags_a{ 0x5C };
			static constexpr std::ptrdiff_t material_flag{ 0x61 };
			static constexpr std::ptrdiff_t flags_b{ 0x62 };
			static constexpr std::ptrdiff_t flags_c{ 0x64 };
		};

		using CBaseSceneData = mesh_draw_primitive_t;

		[[nodiscard]] constexpr std::uintptr_t at( std::uintptr_t batch, int index ) noexcept
		{
			return batch + static_cast< std::size_t >( index ) * mesh_draw_primitive_t::stride;
		}

	}

    class weather
    {
    public:
        void on_frame_stage_notify( );
        void release( );
        void forget( );

    private:
        static constexpr std::uint32_t invalid_effect_index{ static_cast<std::uint32_t>( -1 ) };

        void create_particle( );
        void update_particles( );
        void release_particles( );

        std::uint32_t m_effect_index{ invalid_effect_index };
        int m_last_particle_type{ -1 };
        float m_last_round_start_time{};
        bool m_particle_loaded{};
    };

    class scene
    {
    public:
        struct skybox_entry
        {
            std::string display_name;
            std::string resource_path;
        };

        void discover_skyboxes( );
        void reset_skybox_state( );
        void invalidate_runtime( bool drop_material_cache );

        void on_frame_stage_notify( );
        void on_round_start( );
        void on_draw_skybox_array_pre( std::uintptr_t mesh_array, int mesh_count );
        void on_draw_skybox_array_post( );
        void on_light_scene_object_pre( std::uintptr_t object ) const;
        void on_light_scene_object_post( std::uintptr_t object ) const;
		void on_draw_scene_object_array( std::uintptr_t object_array ) const;
        void on_draw_scene_object( std::uintptr_t batch, int batch_count ) const;
        [[nodiscard]] static bool world_material_swap_active( ) noexcept;
        [[nodiscard]] std::uintptr_t force_world_material_rcx( std::uintptr_t mesh_entry );
        [[nodiscard]] bool on_setup_fog( __m128i* output, int* mode ) const;
        void on_set_shader_param( __m128i*& value, std::uint32_t hash ) const;

        [[nodiscard]] bool settling( ) const noexcept
        {
            const auto now = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );
            return now < this->m_scene_settle_until;
        }

       [[nodiscard]] const std::vector<skybox_entry>& get_skyboxes( ) const { return this->m_skyboxes; }

    private:
        void clear_active_skybox( );
        void load_skybox_material( const char* path );
        [[nodiscard]] float get_night_exposure_scale( ) const;
        void apply_worldblur_param( __m128i*& value, std::uint32_t hash ) const;
        void apply_bloom_param( __m128i*& value, std::uint32_t hash ) const;
        void apply_gamma_param( __m128i*& value, std::uint32_t hash ) const;
        void apply_wetness_param( __m128i*& value, std::uint32_t hash ) const;

        std::vector<skybox_entry> m_skyboxes{};
        struct cached_skybox_material
        {
            std::uintptr_t texture_binding{};
            std::uintptr_t material{};
        };

        std::unordered_map<std::string, cached_skybox_material> m_skybox_materials{};
        std::uintptr_t m_custom_sky_material{};
        int m_loaded_skybox_index{ -1 };

        std::uintptr_t m_active_material_binding{};
        std::uintptr_t m_active_original_material{};
        std::uintptr_t m_active_skybox_descriptor{};
        std::array<float, 3> m_active_original_sky_color{};
        bool m_active_sky_tinted{};

        std::uintptr_t m_world_mat_slot{};
        settings::world::scene::world_engine_mat m_world_mat_id{};
        float m_last_round_start_time{};
        std::int64_t m_scene_settle_until{};

    };

    class smoke
    {
    public:
        void on_render_smoke_pre( ) { this->m_active = true; }
        void on_render_smoke_post( ) { this->m_active = false; }

        void on_map( std::uintptr_t token, std::size_t size, std::uintptr_t buf_ptr );
        void on_unmap( std::uintptr_t token );

    private:
        static inline bool m_active{};
        static inline std::uintptr_t m_buf{};
        static inline std::uintptr_t m_token{};
    };

}

