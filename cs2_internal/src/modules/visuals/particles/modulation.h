#pragma once

#include <core/memory.hpp>
#include <core/settings.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace features::world {

	namespace particle_modulation {

		enum class kind : std::uint8_t
		{
			unknown,
			molotov,
			explosion,
			taser,
			muzzle
		};

		inline bool contains( const char* text, std::string_view token )
		{
			return text && std::strstr( text, token.data( ) ) != nullptr;
		}

		inline bool is_project_owned_effect( const char* name )
		{
			return contains( name, "particles/entity/spectator_utility_trail" ) ||
				contains( name, "spectator_utility_trail" ) ||
				contains( name, "particles/inferno_fx/explosion_incend_air_falling" ) ||
				contains( name, "explosion_incend_air_falling" ) ||
				contains( name, "particles/breakable_fx/breakable_mail_debris_01" ) ||
				contains( name, "breakable_mail_debris_01" ) ||
				contains( name, "particles/explosions_fx/explosion_c4_500" ) ||
				contains( name, "explosion_c4_500" ) ||
				contains( name, "particles/ambient_fx/ghost_player_whisps" ) ||
				contains( name, "particles/critters/chicken/chicken_gone_feathers" ) ||
				contains( name, "particles/rain_fx/ash_burning" ) ||
				contains( name, "particles/explosions_fx/explosion_hegrenade_snow" ) ||
				contains( name, "particles/burning_fx/" ) ||
				contains( name, "particles/water_fx/" ) ||
				contains( name, "particles/weapons/cs_weapon_fx/weapon_sensorgren_debris" ) ||
				contains( name, "particles/ambient_fx/ambient_ground_dust" ) ||
				contains( name, "particles/ambient_fx/ambient_sparks_glow" ) ||
				contains( name, "particles/maps/de_dust/dust_embers" ) ||
				contains( name, "particles/maps/cs_office/office_leak_steam" ) ||
				contains( name, "particles/embedded/" );
		}

		inline bool is_smoke_grenade_effect( const char* name )
		{
			return contains( name, "particles/explosions_fx/explosion_smokegrenade" ) ||
				contains( name, "explosion_smokegrenade" ) ||
				contains( name, "particle_smokegrenade" ) ||
				contains( name, "smokegrenade" ) ||
				contains( name, "smoke_grenade" ) ||
				contains( name, "grenade_smoke" );
		}

		inline const char* read_name( std::uintptr_t data )
		{
			const auto first = memory::safe_read<std::uintptr_t>( data + 0x48 );
			if ( !first || *first < 0x10000ull || *first > 0x00007FFFFFFFFFFFull )
				return nullptr;

			const auto second = memory::safe_read<std::uintptr_t>( *first + 0x18 );
			if ( !second || *second < 0x10000ull || *second > 0x00007FFFFFFFFFFFull )
				return nullptr;

			const auto third = memory::safe_read<std::uintptr_t>( *second + 0x8 );
			if ( !third || *third < 0x10000ull || *third > 0x00007FFFFFFFFFFFull )
				return nullptr;

			const auto name = memory::safe_read<const char*>( *third );
			if ( !name || !*name )
				return nullptr;

			return *name;
		}

		inline kind classify_name( const char* name )
		{
			if ( !name || is_project_owned_effect( name ) || is_smoke_grenade_effect( name ) )
				return kind::unknown;

			if ( contains( name, "muzzle" ) ||
				contains( name, "muzzleflash" ) ||
				contains( name, "muzzle_flash" ) ||
				contains( name, "weapon_muzzle" ) ||
				contains( name, "weapon_muzzleflash" ) ||
				contains( name, "weapon_flash" ) ||
				contains( name, "firstperson_weapon" ) ||
				contains( name, "fp_weapon" ) )
			{
				return kind::muzzle;
			}

			if ( contains( name, "inferno_fx" ) ||
				contains( name, "molotov" ) ||
				contains( name, "incendiary" ) ||
				contains( name, "explosion_molotov_air" ) ||
				contains( name, "explosion_incend_air" ) ||
				contains( name, "weapon_molotov_" ) ||
				contains( name, "weapon_incend_" ) )
			{
				return kind::molotov;
			}

			if ( contains( name, "explosion" ) ||
				contains( name, "explosion_hegrenade" ) ||
				contains( name, "explosion_basic" ) ||
				contains( name, "hegrenade_" ) )
			{
				return kind::explosion;
			}

			if ( contains( name, "taser" ) ||
				contains( name, "weapon_tracers_taser" ) ||
				contains( name, "impact_taser_bodyfx" ) )
			{
				return kind::taser;
			}

			return kind::unknown;
		}

		inline const xdraw::color* resolve_color( kind k )
		{
			const auto& cfg = settings::g_world.m_particles;
			if ( !cfg.modulation.value )
				return nullptr;

			switch ( k )
			{
			case kind::molotov:
				return cfg.molotov.value ? &cfg.molotov_color.value : nullptr;
			case kind::explosion:
				return cfg.explosion.value ? &cfg.explosion_color.value : nullptr;
			case kind::taser:
				return cfg.taser.value ? &cfg.taser_color.value : nullptr;
			case kind::muzzle:
				return cfg.muzzle.value ? &cfg.muzzle_color.value : nullptr;
			default:
				return nullptr;
			}
		}

		inline void apply( void* particle_data )
		{
			const auto& cfg = settings::g_world.m_particles;
			if ( !cfg.modulation.value || !particle_data )
				return;

			const auto data = reinterpret_cast< std::uintptr_t >( particle_data );
			const auto name = read_name( data );
			const auto* color = resolve_color( classify_name( name ) );
			if ( !color )
				return;

			const auto floats = color->to_float( );
			auto* output = reinterpret_cast< float* >( data + 0x50 );
			output[ 0 ] = std::clamp( floats[ 0 ], 0.0f, 1.0f );
			output[ 1 ] = std::clamp( floats[ 1 ], 0.0f, 1.0f );
			output[ 2 ] = std::clamp( floats[ 2 ], 0.0f, 1.0f );
			output[ 3 ] = std::clamp( floats[ 3 ], 0.0f, 1.0f );
		}

	} // namespace particle_modulation

} // namespace features::world
