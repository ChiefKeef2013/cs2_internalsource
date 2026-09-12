#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>

#include "misc.hpp"

namespace features::misc {

	void removals::on_prepare_scene_material( std::uintptr_t material ) const
	{
		if ( !material || !settings::g_misc.m_removals.skybox_fog.value )
		{
			return;
		}

		const auto parameters = memory::read<std::uintptr_t>( material + 0x20 );
		if ( !parameters )
		{
			return;
		}

		const auto count = memory::read<std::uint32_t>( material + 0x18 );

		for ( auto i = 0u; i < count; i++ )
		{
			const auto parameter = parameters + ( static_cast< std::size_t >( i ) * 0x40 );
			const auto name_ptr = memory::read<const char*>( parameter + 0x28 );

			if ( !name_ptr )
			{
				continue;
			}

			const auto param_hash = fnv1a::runtime_hash( name_ptr );
			if ( param_hash == "g_flBrightnessExposureBias"_hash || param_hash == "g_flRenderOnlyExposureBias"_hash )
			{
				memory::write<float>( parameter, -6.0f );
			}
		}
	}

	void removals::on_override_view( std::uintptr_t view_setup ) const
	{
		if ( addresses::globals::cvar )
		{
			if ( auto* cv = addresses::globals::cvar->find( "mat_fullbright"_hash ) )
				cv->m_value.i32 = settings::g_misc.m_removals.fullbright.value ? 1 : 0;
		}

		if ( !settings::g_misc.m_removals.recoil.value )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || systems::g_local.is_in_cinematic( ) || local.team < 2 )
		{
			return;
		}

		memory::write<math::vector3>( view_setup + 0x4b8, systems::g_input.get_view_angles( ) );
	}

}

