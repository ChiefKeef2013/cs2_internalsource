#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/threadpool/threadpool.cpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/common.hpp>

namespace features::combat {

	bool rage::try_auto_scope( systems::input::usercmd* cmd ) const
	{
		if ( !cmd || !settings::g_combat.m_autos.scope.value )
		{
			return false;
		}

		const auto& ctx = g_shared.ctx( );
		if ( ctx.weapon_type != cstypes::weapon_type::sniper || ctx.is_scoped )
		{
			return false;
		}

		cmd->buttons.value |= cstypes::command_buttons::in_second_attack;
		cmd->buttons.value_changed |= cstypes::command_buttons::in_second_attack;
		cmd->buttons.value_scroll |= cstypes::command_buttons::in_second_attack;

		const auto history_index = cmd->csgo_user_cmd.input_history_size( ) - 1;
		if ( history_index >= 0 )
		{
			cmd->csgo_user_cmd.set_attack2_start_history_index( history_index );
		}

		return true;
	}

}


