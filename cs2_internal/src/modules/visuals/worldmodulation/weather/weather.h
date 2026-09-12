#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <modules/visuals/worldmodulation/world.hpp>

namespace features::world {
	namespace {
		struct particle_transform {
			float px{};
			float py{};
			float pz{};
			float pw{};
			float qx{};
			float qy{};
			float qz{};
			float qw{ 1.0f };
		};
		static_assert (sizeof (particle_transform) == 0x20);
	}

	void weather::on_frame_stage_notify( )
	{
		if ( !settings::g_world.m_weather.enabled.value )
		{
			if ( this->m_effect_index != invalid_effect_index )
			{
				this->release_particles( );
			}

			this->m_last_particle_type = -1;
			return;
		}

		const auto game_rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
		if ( !game_rules )
		{
			this->release_particles( );
			return;
		}

		const auto round_start_time = reinterpret_cast<C_CSGameRules*>( game_rules )->m_fRoundStartTime();
		if ( round_start_time != this->m_last_round_start_time )
		{
			this->release_particles( );
			this->m_last_round_start_time = round_start_time;
		}

		this->update_particles( );
	}

	void weather::release( )
	{
		this->release_particles( );
	}

	void weather::forget( )
	{
		this->m_effect_index = invalid_effect_index;
		this->m_last_particle_type = -1;
		this->m_particle_loaded = false;
	}

	void weather::create_particle( )
	{
		const auto particle_manager = memory::read<std::uintptr_t>( addresses::globals::particle_manager );
		if ( !particle_manager )
		{
			return;
		}

		std::string particle_path = "";

		switch ( settings::g_world.m_weather.type )
		{
		case settings::world::weather::weather_type::snow:
			particle_path = xs( "particles/embedded/snow.vpcf" );
			break;
		case settings::world::weather::weather_type::rain:
			particle_path = xs( "particles/embedded/rain.vpcf" );
			break;
		case settings::world::weather::weather_type::stars:
			particle_path = xs( "particles/embedded/stars.vpcf" );
			break;
		case settings::world::weather::weather_type::ss_rain:
			particle_path = xs( "particles/embedded/ss_rain.vpcf" );
			break;
		default:
			return;
		}

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
		} buffer;

		memory::call<void>(PATTERN(PATTERN_INIT_PARTICLE_PATH_BUFFER_ALT), &buffer, particle_path.c_str() );

		buffer.m_unknown4 = 'fcpv';

		memory::call<void>(PATTERN(PATTERN_RESOURCE_SYSTEM_LOAD), addresses::globals::resource_system, &buffer, "" );

		auto effect_index{ invalid_effect_index };
		memory::call<int*>(PATTERN(PATTERN_PARTICLE_CREATE_EFFECT), particle_manager, &effect_index, particle_path.c_str (), 8, 0ll, 0ll, 0ll, 0 );

		this->m_effect_index = effect_index;
		this->m_last_particle_type = static_cast< int >( settings::g_world.m_weather.type.value );
	}

	void weather::update_particles( )
	{
		const auto current_type = static_cast< int >( settings::g_world.m_weather.type.value );

		if ( this->m_effect_index != invalid_effect_index && this->m_last_particle_type != current_type )
		{
			this->release_particles( );
		}

		if ( this->m_effect_index == invalid_effect_index )
		{
			this->create_particle( );

			if ( this->m_effect_index == invalid_effect_index )
			{
				return;
			}
		}

		const auto particle_manager = memory::read<std::uintptr_t>( addresses::globals::particle_manager );
		const auto view_pawn = systems::g_local.get( ).view_pawn( );

		if ( !view_pawn || !particle_manager )
		{
			return;
		}

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( view_pawn )->m_pGameSceneNode();
		if ( !game_scene_node )
		{
			return;
		}

		const auto origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin();
		const auto& weather = settings::g_world.m_weather;
		const auto type = weather.type.value;
		const auto windy =
			weather.wind.value &&
			( type == settings::world::weather::weather_type::rain ||
			  type == settings::world::weather::weather_type::ss_rain );

		if ( windy )
		{
			const auto direction = weather.wind_direction.value * ( std::numbers::pi_v<float> / 180.0f );
			const auto strength = std::clamp( weather.wind_strength.value / 5.0f, 0.0f, 1.0f );
			const auto turbulence = std::clamp( weather.wind_turbulence.value / 5.0f, 0.0f, 1.0f );
			float vx = strength * std::cos( direction );
			float vy = strength * std::sin( direction );

			if ( turbulence > 0.0f )
			{
				const auto time = static_cast<double>( GetTickCount64( ) ) * 0.0006;
				const auto noise_x = static_cast<float>(
					0.60 * std::sin( time * 0.90 + 0.3 ) +
					0.30 * std::sin( time * 2.30 + 1.7 ) +
					0.15 * std::sin( time * 5.10 + 4.2 ) );
				const auto noise_y = static_cast<float>(
					0.60 * std::sin( time * 1.10 + 2.0 ) +
					0.30 * std::sin( time * 2.70 + 0.5 ) +
					0.15 * std::sin( time * 4.60 + 3.1 ) );
				vx += turbulence * 0.9f * noise_x;
				vy += turbulence * 0.9f * noise_y;
			}

			const auto magnitude = std::min( std::sqrt( vx * vx + vy * vy ), 1.0f );
			const auto tilt = magnitude * 80.0f * ( std::numbers::pi_v<float> / 180.0f );
			const auto heading = vx != 0.0f || vy != 0.0f ? std::atan2( vy, vx ) : 0.0f;
			const auto sine = std::sin( tilt * 0.5f );
			const particle_transform transform{
				origin.x, origin.y, origin.z, 0.0f,
				std::sin( heading ) * sine,
				-std::cos( heading ) * sine,
				0.0f,
				std::cos( tilt * 0.5f )
			};
			memory::call<bool>( PATTERN(PATTERN_PARTICLE_SET_TRANSFORM), particle_manager, this->m_effect_index, 0, &transform, 0 );
		}
		else
		{
			memory::call<bool>( PATTERN(PATTERN_PARTICLE_SET_CONTROL_POINT), particle_manager, this->m_effect_index, 0, &origin, 0 );
		}

		const auto color = math::vector3{ static_cast< float >( settings::g_world.m_weather.color.value.r ), static_cast< float >( settings::g_world.m_weather.color.value.g ), static_cast< float >( settings::g_world.m_weather.color.value.b ) };
		memory::call<bool>(PATTERN(PATTERN_PARTICLE_SET_CONTROL_POINT), particle_manager, this->m_effect_index, 1, &color, 0 );
	}

	void weather::release_particles( )
	{
		if ( this->m_effect_index == invalid_effect_index )
		{
			return;
		}

		const auto particle_manager = memory::read<std::uintptr_t>( addresses::globals::particle_manager );
		if ( particle_manager )
		{
			memory::call<void>(PATTERN(PATTERN_PARTICLE_DESTROY_EFFECT), particle_manager, this->m_effect_index, true, true );
		}

		this->m_effect_index = invalid_effect_index;
		this->m_last_particle_type = -1;
		this->m_particle_loaded = false;
	}

}



