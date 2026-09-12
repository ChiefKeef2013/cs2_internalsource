#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {

	void killfeed::on_frame_stage_notify( )
	{
		const auto local = systems::g_local.get( );

		if ( !local.pawn || !local.is_alive )
		{
			return;
		}

		const auto hud_element = memory::call<std::uintptr_t>( PATTERN( PATTERN_FIND_HUD_ELEMENT ), xs( "CCSGO_HudDeathNotice" ) );
		if ( !hud_element )
		{
			return;
		}

		memory::write<float>( hud_element + 0x58, settings::g_misc.preserve_killfeed ? 1000.0f : 1.5f );

		const auto spawntime = reinterpret_cast< C_CSPlayerPawnBase* >( local.pawn )->m_flLastSpawnTimeIndex( );
		if ( this->m_last_spawntime != spawntime )
		{
			const auto clear_death_notices = PATTERN( PATTERN_HUD_DEATH_NOTICE_CLEAR );
			if ( clear_death_notices )
			{
				memory::call<void>( clear_death_notices, hud_element - 0x20 );
			}

			this->m_last_spawntime = spawntime;
		}
	}

}

