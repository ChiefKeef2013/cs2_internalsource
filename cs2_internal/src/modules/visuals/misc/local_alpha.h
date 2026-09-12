#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

namespace features::misc {

	void local_alpha::on_frame_stage_notify( )
	{
		const auto local = systems::g_local.get( );

		if ( !local.pawn || !local.is_alive )
		{
			if ( this->m_is_alpha_changed && local.pawn )
			{
				memory::call<void>( PATTERN( PATTERN_GAME_EVENT_GET_STRING ), local.pawn, 255 );
			}

			this->m_is_alpha_changed = false;
			return;
		}

		if ( !settings::g_esp.m_local_alpha.enabled.value )
		{
			if ( this->m_is_alpha_changed )
			{
				this->m_is_alpha_changed = false;
				memory::call<void>( PATTERN( PATTERN_GAME_EVENT_GET_STRING ), local.pawn, 255 );
			}

			return;
		}

		const auto is_scoped = systems::g_entities.is_cs_player_pawn( local.pawn )
			&& reinterpret_cast< C_CSPlayerPawn* >( local.pawn )->m_bIsScoped( );
		const auto should_apply = !settings::g_esp.m_local_alpha.only_scoped.value || is_scoped;

		if ( should_apply )
		{
			this->m_is_alpha_changed = true;
			const auto alpha = static_cast< std::uint8_t >( settings::g_esp.m_local_alpha.opacity.value * 255.0f );
			memory::call<void>( PATTERN( PATTERN_GAME_EVENT_GET_STRING ), local.pawn, alpha );
		}
		else if ( this->m_is_alpha_changed )
		{
			this->m_is_alpha_changed = false;
			memory::call<void>( PATTERN( PATTERN_GAME_EVENT_GET_STRING ), local.pawn, 255 );
		}
	}

}

