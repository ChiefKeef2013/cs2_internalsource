#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/threadpool/threadpool.cpp>
#include <core/features.hpp>
#include <modules/rage/misc/seed/nospread_seed.h>

namespace features::combat {

	inline std::pair<int, float> advance_tick( int tick, float frac, int dt, float df )
	{
		frac += df;
		const auto carry = static_cast< int >( std::floor( frac ) );
		frac -= static_cast< float >( carry );
		return { tick + dt + carry, frac };
	}

	inline void reset_input_interp( auto* entry )
	{
		if ( entry->has_sv_interp0( ) )
		{
			const auto interp = entry->mutable_sv_interp0( );
			interp->set_src_tick( -1 );
			interp->set_dst_tick( -1 );
			interp->set_frac( 0.0f );
		}

		if ( entry->has_sv_interp1( ) )
		{
			const auto interp = entry->mutable_sv_interp1( );
			interp->set_src_tick( -1 );
			interp->set_dst_tick( -1 );
			interp->set_frac( 0.0f );
		}

		if ( entry->has_cl_interp( ) )
		{
			const auto interp = entry->mutable_cl_interp( );
			interp->set_frac( 0.0f );
		}
	}

	bool rage::process_doubletap( systems::input::usercmd* cmd, const systems::local::snapshot& local, bool charge_dt )
	{
		if ( !charge_dt )
		{
			return false;
		}

		const auto& shared_ctx = g_shared.ctx( );
		const auto& config = settings::g_combat.m_ragebot.get_group( shared_ctx.weapon_type, shared_ctx.item_def_idx );
		if ( !config.doubletap.value )
		{
			return false;
		}

		if ( !cmd || !shared_ctx.weapon || !local.controller )
		{
			return false;
		}

		const auto base_cmd = cmd->csgo_user_cmd.mutable_base( );
		if ( !base_cmd )
		{
			return false;
		}

		const auto client_tick = base_cmd->client_tick( );
		const auto next_primary = memory::read<int>( shared_ctx.weapon + SCHEMA( "C_BasePlayerWeapon", "m_nNextPrimaryAttackTick"_hash ) );
		const auto next_ratio = memory::read<float>( shared_ctx.weapon + SCHEMA( "C_BasePlayerWeapon", "m_flNextPrimaryAttackTickRatio"_hash ) );
		const auto tick_base = memory::read<int>( local.controller + SCHEMA( "CBasePlayerController", "m_nTickBase"_hash ) );
		const auto in_reload = memory::read<bool>( shared_ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_bInReload"_hash ) );
		const auto clip = memory::read<int>( shared_ctx.weapon + SCHEMA( "C_BasePlayerWeapon", "m_iClip1"_hash ) );

		const auto can_attack =
			client_tick >= next_primary &&
			tick_base > g_shared.last_shoot_tick( ) &&
			!in_reload &&
			clip > 0;

		if ( !can_attack )
		{
			return false;
		}

		const auto ratio_clean = std::isfinite( next_ratio ) ? next_ratio : 0.0f;
		double offset_tick_d = 0.0;
		const double frac = std::modf( static_cast<double>( ratio_clean ), &offset_tick_d );
		auto base_tick = next_primary + static_cast<int>( offset_tick_d );
		if ( frac >= 1.0 )
		{
			++base_tick;
		}
		else if ( frac < 0.0 )
		{
			--base_tick;
		}

		auto server_tick = tick_base;
		if ( const auto net_client = addresses::globals::network_client_service )
		{
			if ( const auto tick_state = memory::call_vfunc<std::uintptr_t>( net_client, cstypes::offsets::net_tick_info_vfunc ) )
			{
				const auto live_server_tick = memory::read<int>( tick_state + cstypes::offsets::net_server_tick );
				if ( live_server_tick > 0 )
				{
					server_tick = live_server_tick;
				}
			}
		}

		const auto first_shot_tick = std::max( base_tick, server_tick + 1 );
		const auto second_shot_tick = first_shot_tick + 1;

		const auto initial_size = cmd->csgo_user_cmd.input_history_size( );

		if ( initial_size < 1 )
		{
			return false;
		}

		const auto first_idx = initial_size - 1;

		if ( auto* entry_first = cmd->csgo_user_cmd.mutable_input_history( first_idx ) )
		{
			entry_first->set_player_tick_count( first_shot_tick );
			entry_first->set_player_tick_fraction( 0.0f );
		}

		auto* hist = cmd->csgo_user_cmd.mutable_input_history( );
		auto* entry_second = hist ? hist->add( ) : nullptr;

		if ( !entry_second )
		{

			cmd->buttons.value |= cstypes::command_buttons::in_attack;
			cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
			cmd->buttons.value_scroll |= cstypes::command_buttons::in_attack;
			cmd->csgo_user_cmd.set_attack1_start_history_index( first_idx );
			return true;
		}

		entry_second->set_player_tick_count( second_shot_tick );
		entry_second->set_player_tick_fraction( 0.499999f );

		if ( auto* entry_first = cmd->csgo_user_cmd.mutable_input_history( first_idx ) )
		{
			if ( const auto va = entry_first->view_angles( ) )
			{
				if ( auto* va2 = entry_second->mutable_view_angles( ) )
				{
					va2->set_x( va->x( ) );
					va2->set_y( va->y( ) );
					va2->set_z( va->z( ) );
				}
			}
			if ( entry_first->has_render_tick_count( ) )
			{
				entry_second->set_render_tick_count( entry_first->render_tick_count( ) );
			}
			if ( entry_first->has_render_tick_fraction( ) )
			{
				entry_second->set_render_tick_fraction( entry_first->render_tick_fraction( ) );
			}
		}

		cmd->buttons.value |= cstypes::command_buttons::in_attack;
		cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
		cmd->buttons.value_scroll |= cstypes::command_buttons::in_attack;

		cmd->csgo_user_cmd.set_attack1_start_history_index( first_idx );


		return true;
	}

	void rage::auto_revolver( systems::input::usercmd* cmd, const aim_context& ctx, const systems::local::snapshot& local )
	{
		if ( !settings::g_combat.m_ragebot.enabled )
		{
			this->m_revolver_cock_ticks = 0;
			return;
		}

		if ( !g_shared.can_shoot( cmd, local.controller ) )
		{
			this->m_revolver_cock_ticks = 0;
			return;
		}

		if ( !settings::g_combat.m_autos.revolver.value )
		{
			this->m_revolver_cock_ticks = 0;
			return;
		}

		constexpr auto cock_ticks{ 13 };
		if ( this->m_revolver_cock_ticks >= cock_ticks )
		{

			cmd->buttons.value &= ~cstypes::command_buttons::in_attack;
			cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
			cmd->buttons.value_scroll &= ~cstypes::command_buttons::in_attack;
			cmd->csgo_user_cmd.set_attack1_start_history_index( -1 );
			this->m_revolver_cock_ticks = 0;

			this->run_gun( cmd, ctx, local );
			return;
		}

		this->run_gun( cmd, ctx, local, false );

		cmd->buttons.value |= cstypes::command_buttons::in_attack;
		cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
		cmd->buttons.value_scroll |= cstypes::command_buttons::in_attack;

		const auto history_index = cmd->csgo_user_cmd.input_history_size( ) - 1;
		if ( history_index >= 0 )
		{
			cmd->csgo_user_cmd.set_attack1_start_history_index( history_index );
		}

		++this->m_revolver_cock_ticks;
	}

	void rage::fire_gun( systems::input::usercmd* cmd, const target& tgt, bool was_forced, const math::vector3& shoot_eye, const systems::local::snapshot& local, bool subtick_attack )
	{
		if ( !tgt.hit.record || !tgt.hit.record->valid )
		{
			return;
		}

		this->m_firing_this_tick = true;

		const auto base = cmd->csgo_user_cmd.mutable_base( );
		const auto tick_base = memory::read<int>( local.controller + SCHEMA( "CBasePlayerController", "m_nTickBase"_hash ) );
		const auto& shared_ctx = g_shared.ctx( );
		const auto& config = settings::g_combat.m_ragebot.get_group( shared_ctx.weapon_type, shared_ctx.item_def_idx );
		auto aim_punch = g_shared.get_aim_punch( local.pawn );
		auto aim_angle = config.no_spread.value ? math::helpers::calculate_angle( shoot_eye, tgt.hit.position ) : tgt.hit.aim_angle;

		if ( config.no_spread.value )
		{
			auto stamp_tick = tick_base;
			auto stamp_frac{ 0.0f };

			if ( !tgt.hit.source_eye.is_uninterpolated )
			{
				std::tie( stamp_tick, stamp_frac ) = advance_tick( tgt.hit.source_eye.player_tick, tgt.hit.source_eye.player_frac, tgt.hit.source_eye.lerp_ticks_int, tgt.hit.source_eye.lerp_ticks_frac );
			}

			// Use seed tick's punch for correct recoil compensation (critical for Deagle/R8).
			math::vector3 seed_punch{};
			if ( hitchance::read_seed_fire_punch( local.pawn, shared_ctx.weapon, stamp_tick, stamp_frac, seed_punch ) )
			{
				seed_punch.z = 0.0f;
				aim_punch = seed_punch;
			}

			math::vector3 corrected{};
			if ( !g_shared.find_spread_correction( aim_angle, stamp_tick, corrected ) )
			{
				this->m_firing_this_tick = false;
				return;
			}

			aim_angle = corrected;
		}

		g_shared.last_shoot_tick( ) = tick_base;

		features::misc::g_impacts.on_boom( tgt.hit.pawn, tgt.hit.hitgroup, tgt.hit.damage, tgt.hitchance, shared_ctx.inaccuracy, shared_ctx.spread, aim_angle, shoot_eye, tgt.hit.record->tick, g_shared.lc( ).get_skeleton( *tgt.hit.record ), was_forced );

		this->push_visualize( tgt.hit );
		if ( tgt.hit.record->bone_count > 0 )
		{
			features::esp::player::g_chams.os( ).push( tgt.hit.pawn, tgt.hit.record->bones, tgt.hit.record->bone_count );
		}
		else
		{
			features::esp::player::g_chams.os( ).push( tgt.hit.pawn );
		}
		const auto resolve_record_time = [ & ]( ) -> std::optional<cstypes::tick_fraction>
		{
			if ( !tgt.hit.record || !tgt.hit.record->valid )
			{
				return std::nullopt;
			}

			if ( tgt.hit.record->extrapolated )
			{
				return std::nullopt;
			}

			cstypes::tick_fraction tf{};
			tf.tick = tgt.hit.record->tick;
			tf.frac = 0.0f;
			return tf;
		};

		const auto record_time = resolve_record_time( );
		const auto interp_ticks = tgt.hit.source_eye.is_uninterpolated
			? 1
			: std::max( 1, tgt.hit.source_eye.lerp_ticks_int + ( tgt.hit.source_eye.lerp_ticks_frac > 0.5f ? 1 : 0 ) );
		const auto history_size = cmd->csgo_user_cmd.input_history_size( );
		for ( auto i = 0; i < history_size; ++i )
		{
			const auto entry = cmd->csgo_user_cmd.mutable_input_history( i );
			if ( !entry )
			{
				continue;
			}

			if ( const auto angles = entry->mutable_view_angles( ) )
			{
				angles->set_x( aim_angle.x - aim_punch.x );
				angles->set_y( aim_angle.y - aim_punch.y );

				if ( config.no_spread.value )
				{
					angles->set_z( aim_angle.z );
				}
			}

			if ( record_time.has_value( ) )
			{
				entry->set_render_tick_count( record_time->tick + interp_ticks );
				entry->set_render_tick_fraction( 0.0f );
			}

			if ( !subtick_attack && !tgt.hit.source_eye.is_uninterpolated )
			{
				const auto [stamp_tick, stamp_frac] = advance_tick( tgt.hit.source_eye.player_tick, tgt.hit.source_eye.player_frac, tgt.hit.source_eye.lerp_ticks_int, tgt.hit.source_eye.lerp_ticks_frac );

				entry->set_player_tick_count( stamp_tick );
				entry->set_player_tick_fraction( stamp_frac );
			}

			reset_input_interp( entry );
		}

		if ( !subtick_attack )
		{
			cmd->buttons.value |= cstypes::command_buttons::in_attack;
			cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
			cmd->buttons.value_scroll |= cstypes::command_buttons::in_attack;

			if ( history_size > 0 )
			{
				cmd->csgo_user_cmd.set_attack1_start_history_index( history_size - 1 );
			}
		}

		math::vector3 forward{};
		{
			if ( const auto angles = base->viewangles( ) )
			{
				math::helpers::angle_vectors_left( { angles->x( ), angles->y( ), angles->z( ) }, &forward );
			}
		}

		const auto punched_aim = math::vector3{ aim_angle.x - aim_punch.x, aim_angle.y - aim_punch.y, 0.0f };
		const auto facing_away = forward.dot( ( tgt.hit.record->origin - systems::g_prediction.pre( ).networked_origin ).normalized( ) ) < 0.707107f;

		auto command_aim = punched_aim;
		if ( !subtick_attack && facing_away && settings::g_combat.m_antiaim.hide_shots.value )
		{
			command_aim.x = 179.9f;
			command_aim.y = std::remainderf( punched_aim.y + 180.0f, 360.0f );
		}

		if ( const auto angles = base->mutable_viewangles( ) )
		{
			angles->set_x( command_aim.x );
			angles->set_y( command_aim.y );
		}

		if ( !config.silent.value )
		{
			systems::g_input.set_view_angles( punched_aim );
		}
	}

	void rage::fire_melee( systems::input::usercmd* cmd, const target& tgt, const systems::local::snapshot& local )
	{
		if ( !tgt.hit.record || !tgt.hit.record->valid )
		{
			return;
		}

		this->m_firing_this_tick = true;

		const auto base = cmd->csgo_user_cmd.mutable_base( );
		const auto tick_base = memory::read<int>( local.controller + SCHEMA( "CBasePlayerController", "m_nTickBase"_hash ) );

		g_shared.last_shoot_tick( ) = tick_base;

		cstypes::tick_fraction record_time{};
		record_time.tick = tgt.hit.record->tick;
		record_time.frac = 0.0f;
		const auto history_index = cmd->csgo_user_cmd.input_history_size( ) - 1;
		const auto entry = history_index >= 0 ? cmd->csgo_user_cmd.mutable_input_history( history_index ) : nullptr;

		if ( entry )
		{
			if ( const auto angles = entry->mutable_view_angles( ) )
			{
				angles->set_x( tgt.hit.aim_angle.x );
				angles->set_y( tgt.hit.aim_angle.y );
			}

			if ( !tgt.hit.record->extrapolated )
			{
				entry->set_render_tick_count( record_time.tick + 1 );
				entry->set_render_tick_fraction( 0.0f );
			}

			if ( !tgt.hit.source_eye.is_uninterpolated )
			{
				const auto [stamp_tick, stamp_frac] = advance_tick( tgt.hit.source_eye.player_tick, tgt.hit.source_eye.player_frac, tgt.hit.source_eye.lerp_ticks_int, tgt.hit.source_eye.lerp_ticks_frac );

				entry->set_player_tick_count( stamp_tick );
				entry->set_player_tick_fraction( stamp_frac );
			}

			reset_input_interp( entry );
		}

		const auto is_secondary = tgt.hit.attack_type == 1;
		const auto attack_button = is_secondary
			? cstypes::command_buttons::in_second_attack
			: cstypes::command_buttons::in_attack;

		cmd->buttons.value |= attack_button;
		cmd->buttons.value_changed |= attack_button;
		cmd->buttons.value_scroll |= attack_button;

		if ( history_index >= 0 )
		{
			if ( is_secondary )
			{
				cmd->csgo_user_cmd.set_attack2_start_history_index( history_index );
			}
			else
			{
				cmd->csgo_user_cmd.set_attack1_start_history_index( history_index );
			}
		}

		const auto& config = settings::g_combat.m_ragebot.get_group( g_shared.ctx( ).weapon_type, g_shared.ctx( ).item_def_idx );
		if ( !config.silent.value )
		{
			if ( const auto angles = base->mutable_viewangles( ) )
			{
				angles->set_x( tgt.hit.aim_angle.x );
				angles->set_y( tgt.hit.aim_angle.y );
			}

			systems::g_input.set_view_angles( tgt.hit.aim_angle );
		}
	}

}


