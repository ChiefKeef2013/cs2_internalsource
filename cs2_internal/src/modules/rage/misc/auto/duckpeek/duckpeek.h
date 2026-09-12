#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>

namespace features::combat {

	void misc::duckpeek::on_create_move( systems::input::usercmd* cmd )
	{
		this->m_fake_stand_active = false;

		if ( !cmd || !settings::g_combat.m_duckpeek.enabled.value )
		{
			this->m_was_active = false;
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn || systems::g_local.is_in_cinematic( ) || systems::g_local.is_in_time_freeze( ) )
		{
			this->m_was_active = false;
			return;
		}

		this->m_was_active = true;

		if ( g_rage.should_release_duck_for_shot( ) )
		{
			cmd->buttons.value &= ~cstypes::command_buttons::in_duck;
			this->m_fake_stand_active = true;
			return;
		}

		cmd->buttons.value |= cstypes::command_buttons::in_duck;

		if ( g_rage.duckpeek_wants_reduck( ) )
		{
			g_rage.clear_duckpeek_reduck( );
		}
	}

	void misc::duckpeek::on_override_view( std::uintptr_t view_setup )
	{
		if ( !settings::g_combat.m_duckpeek.enabled.value )
		{
			this->m_was_active = false;
			this->m_fake_stand_active = false;
			return;
		}

		if ( !this->m_was_active || !view_setup )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn )
		{
			return;
		}

		constexpr auto standing_view_z = 64.093811f;
		const auto view_offset = reinterpret_cast<C_BaseModelEntity*>( local.pawn )->m_vecViewOffset( );
		const auto lift = standing_view_z - view_offset.z;
		if ( lift <= 0.5f )
		{
			return;
		}

		auto origin = memory::read<math::vector3>( view_setup + 0x4a0 );
		origin.z += lift;
		memory::write<math::vector3>( view_setup + 0x4a0, origin );
	}

}



