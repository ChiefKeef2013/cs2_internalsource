#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/settings.hpp>

#include "movement.hpp"
#include <core/common.hpp>

namespace features::movement {

	namespace {

		constexpr auto k_jump = cstypes::command_buttons::in_jump;

		constexpr std::uintptr_t k_move_svc_buttons = 0x50;

		struct pre_hop_state
		{
			bool space_held{};
			bool want_hop{};
			bool on_ground{};
			bool valid{};
		};

		pre_hop_state g_pre{};
		bool g_was_air{ true };

		[[nodiscard]] bool bad_move_type( std::uintptr_t pawn )
		{
			const auto move_type = pawn
				? memory::read<std::uint8_t>(
					pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_nActualMoveType"_hash ) )
				: 0;
			return move_type == cstypes::move_type::ladder
				|| move_type == cstypes::move_type::noclip
				|| move_type == cstypes::move_type::observer;
		}

		[[nodiscard]] bool live_on_ground( std::uintptr_t pawn )
		{
			const auto flags = pawn
				? memory::read<std::uint32_t>(
					pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_fFlags"_hash ) )
				: 0;
			return ( flags & cstypes::entity_flags::on_ground ) != 0;
		}

		[[nodiscard]] bool soft_land_contact( std::uintptr_t pawn )
		{
			if ( !g_was_air )
			{
				return false;
			}

			const auto ground = pawn
				? memory::read<std::uint32_t>(
					pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_hGroundEntity"_hash ) )
				: 0xffffffffu;
			return ground != 0 && ground != 0xffffffffu;
		}

		[[nodiscard]] std::uintptr_t get_move_svc( std::uintptr_t pawn )
		{
			return pawn
				? memory::read<std::uintptr_t>(
					pawn + SCHEMA_OFFSET( "C_BasePlayerPawn", "m_pMovementServices"_hash ) )
				: 0;
		}

		void write_move_svc_jump( std::uintptr_t svc, bool edge_down )
		{
			if ( !svc )
			{
				return;
			}

			const auto value = memory::read<std::uint64_t>( svc + k_move_svc_buttons );
			const auto pressed = memory::read<std::uint64_t>( svc + k_move_svc_buttons + 0x8 );
			const auto released = memory::read<std::uint64_t>( svc + k_move_svc_buttons + 0x10 );

			if ( edge_down )
			{
				memory::write( svc + k_move_svc_buttons, value | k_jump );
				memory::write( svc + k_move_svc_buttons + 0x8, pressed | k_jump );
				memory::write( svc + k_move_svc_buttons + 0x10, released & ~k_jump );
			}
			else
			{
				memory::write( svc + k_move_svc_buttons, value & ~k_jump );
				memory::write( svc + k_move_svc_buttons + 0x8, pressed & ~k_jump );
				memory::write( svc + k_move_svc_buttons + 0x10, released & ~k_jump );
			}
		}

		void write_pack_jump( bool edge_down, std::uintptr_t pawn )
		{
			write_move_svc_jump( get_move_svc( pawn ), edge_down );
		}

		void write_cmd_jump_edge( systems::input::usercmd* cmd, bool down )
		{
			if ( !cmd )
			{
				return;
			}

			if ( down )
			{
				cmd->buttons.value |= k_jump;
				cmd->buttons.value_changed |= k_jump;
				cmd->buttons.value_scroll &= ~k_jump;
			}
			else
			{
				cmd->buttons.value &= ~k_jump;
				cmd->buttons.value_changed &= ~k_jump;
				cmd->buttons.value_scroll &= ~k_jump;
			}

			if ( const auto base = cmd->csgo_user_cmd.mutable_base( ) )
			{
				if ( const auto bp = base->mutable_buttons_pb( ) )
				{
					auto b1 = bp->buttonstate1( );
					auto b2 = bp->buttonstate2( );
					auto b3 = bp->buttonstate3( );

					if ( down )
					{
						b1 |= k_jump;
						b2 |= k_jump;
						b3 &= ~k_jump;
					}
					else
					{
						b1 &= ~k_jump;
						b2 &= ~k_jump;
						b3 &= ~k_jump;
					}

					bp->set_buttonstate1( b1 );
					bp->set_buttonstate2( b2 );
					bp->set_buttonstate3( b3 );
				}
			}
		}

		void clear_jump_subticks( systems::input::usercmd* cmd )
		{
			if ( !cmd )
			{
				return;
			}

			const auto base = cmd->csgo_user_cmd.mutable_base( );
			if ( !base )
			{
				return;
			}

			auto* moves = base->mutable_subtick_moves( );
			if ( !moves )
			{
				return;
			}

			for ( int i = 0; i < moves->size( ); ++i )
			{
				auto* step = base->mutable_subtick_moves( i );
				if ( !step || step->button( ) != k_jump )
				{
					continue;
				}

				step->set_button( 0 );
				step->set_pressed( false );
				step->set_when( 0.0f );
			}
		}

		[[nodiscard]] std::optional<float> predict_landing_fraction(
			std::uintptr_t local_pawn,
			std::uintptr_t movement_services,
			const systems::prediction::state& prestate,
			bool holding_duck )
		{
			if ( prestate.networked_velocity.z > 0.0f )
			{
				return std::nullopt;
			}

			const auto duck_amount = movement_services
				? memory::read<float>(
					movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flDuckAmount"_hash ) )
				: 0.0f;
			const auto mins = local_pawn
				? memory::read<math::vector3>(
					local_pawn + SCHEMA_OFFSET( "C_BaseModelEntity", "m_Collision"_hash ) + SCHEMA_OFFSET( "CCollisionProperty", "m_vecMins"_hash ) )
				: math::vector3{};
			auto maxs = local_pawn
				? memory::read<math::vector3>(
					local_pawn + SCHEMA_OFFSET( "C_BaseModelEntity", "m_Collision"_hash ) + SCHEMA_OFFSET( "CCollisionProperty", "m_vecMaxs"_hash ) )
				: math::vector3{};

			auto trace_origin = prestate.networked_origin;
			if ( holding_duck && duck_amount > 0.0f )
			{
				constexpr auto standing_height{ 72.0f };
				const auto duck_hull_diff = standing_height - maxs.z;
				trace_origin.z -= duck_hull_diff * 0.5f;
				maxs.z = standing_height;
			}

			auto trace_mask{ 0ull };
			{
				const auto pawn_ptr = movement_services
					? memory::read<std::uintptr_t>( movement_services + 56 )
					: 0;
				if ( pawn_ptr )
				{
					trace_mask = memory::read<std::uintptr_t>( pawn_ptr + 0xd48 );
					if ( memory::read<std::uint32_t>( pawn_ptr + 0x3f8 ) & 0x10 )
					{
						trace_mask |= 0x20;
					}
				}
				else
				{
					trace_mask |= 0x20;
				}
			}

			const auto filter = systems::g_tracing.make_player_movement_filter( local_pawn, trace_mask, 11 );

			const auto sv_gravity_cvar = CONVAR( "sv_gravity" );
			const auto sv_standable_cvar = CONVAR( "sv_standable_normal" );
			if ( !sv_gravity_cvar || !sv_standable_cvar )
			{
				return std::nullopt;
			}

			const auto sv_gravity = sv_gravity_cvar->get<float>( );
			const auto sv_standable_normal = sv_standable_cvar->get<float>( );
			const auto gravity_scale = local_pawn
				? memory::read<float>(
					local_pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_flGravityScale"_hash ) )
				: 1.0f;

			auto velocity = prestate.networked_velocity;
			velocity.z -= ( gravity_scale * sv_gravity * cstypes::tick_interval ) * 0.5f;

			const math::vector3 trace_start = trace_origin;
			math::vector3 trace_end{};
			trace_end.x = trace_origin.x + velocity.x * cstypes::tick_interval;
			trace_end.y = trace_origin.y + velocity.y * cstypes::tick_interval;
			trace_end.z = trace_origin.z + velocity.z * cstypes::tick_interval;
			trace_end.z -= 2.0f;

			const auto result = systems::g_tracing.trace_player_bbox( trace_start, trace_end, { mins, maxs }, filter, movement_services );
			if ( result.fraction <= 0.0f || result.fraction >= 1.0f || result.normal.z < sv_standable_normal )
			{
				return std::nullopt;
			}

			return std::clamp( std::round( result.fraction * 64.0f ) / 64.0f, 1.0f / 64.0f, 63.0f / 64.0f );
		}

		void apply_landing_jump( proto::base_usercmd_pb* base, float when )
		{
			const auto subtick_moves = base->mutable_subtick_moves( );
			const auto release_when = std::clamp( when - 1.0f / 64.0f, 1.0f / 64.0f, 63.0f / 64.0f );

			if ( release_when < when )
			{
				if ( const auto jump_up = systems::g_input.acquire_subtick_step( subtick_moves ) )
				{
					jump_up->set_button( k_jump );
					jump_up->set_pressed( false );
					jump_up->set_when( release_when );
				}
			}

			if ( const auto jump_down = systems::g_input.acquire_subtick_step( subtick_moves ) )
			{
				jump_down->set_button( k_jump );
				jump_down->set_pressed( true );
				jump_down->set_when( when );
			}
		}

	}

	void bhop::pre_create_move( std::uintptr_t  )
	{
		g_pre = {};

		if ( !settings::g_movement.bhop.value )
		{
			return;
		}

		const auto auto_bhop = CONVAR( "sv_autobunnyhopping" );
		if ( auto_bhop && auto_bhop->get<bool>( ) )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || local.team < 2 || !local.pawn || bad_move_type( local.pawn ) )
		{
			return;
		}

		const auto hp = memory::read<std::int32_t>(
			local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_iHealth"_hash ) );
		if ( hp <= 0 || hp > 200 )
		{
			return;
		}

		const bool space = ( ::GetAsyncKeyState( VK_SPACE ) & 0x8000 ) != 0;
		const bool ground = live_on_ground( local.pawn );
		const bool soft = soft_land_contact( local.pawn );
		const bool landing = ground || soft;
		const bool hop = space && landing;

		if ( !landing )
		{
			g_was_air = true;
		}
		else if ( hop || !space )
		{
			g_was_air = false;
		}

		g_pre.space_held = space;
		g_pre.on_ground = ground;
		g_pre.want_hop = hop;
		g_pre.valid = true;

		if ( space || hop )
		{
			write_pack_jump( hop, local.pawn );
		}
		else
		{
			write_pack_jump( false, local.pawn );
		}
	}

	void bhop::on_create_move( systems::input::usercmd* cmd ) const
	{
		if ( !settings::g_movement.bhop.value || !cmd )
		{
			return;
		}

		const auto auto_bhop = CONVAR( "sv_autobunnyhopping" );
		if ( auto_bhop && auto_bhop->get<bool>( ) )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.pawn || !local.is_alive || local.team < 2 || bad_move_type( local.pawn ) )
		{
			return;
		}

		const auto hp = memory::read<std::int32_t>(
			local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_iHealth"_hash ) );
		if ( hp <= 0 || hp > 200 )
		{
			return;
		}

		if ( !g_pre.valid )
		{
			return;
		}

		const bool hop = g_pre.want_hop;
		const bool space = g_pre.space_held
			|| ( cmd->buttons.value & k_jump ) != 0
			|| ( ::GetAsyncKeyState( VK_SPACE ) & 0x8000 ) != 0;

		clear_jump_subticks( cmd );
		write_pack_jump( false, local.pawn );

		if ( hop )
		{
			write_cmd_jump_edge( cmd, true );
			return;
		}

		if ( space )
		{

			write_cmd_jump_edge( cmd, false );
		}

		const auto movement_services = get_move_svc( local.pawn );
		if ( !movement_services || !space )
		{
			return;
		}

		const auto& prestate = systems::g_prediction.pre( );
		const auto holding_duck = ( cmd->buttons.value & cstypes::command_buttons::in_duck ) != 0;
		const auto landing = predict_landing_fraction( local.pawn, movement_services, prestate, holding_duck );
		if ( !landing )
		{
			return;
		}

		const auto base = cmd->csgo_user_cmd.mutable_base( );
		if ( !base )
		{
			return;
		}

		write_cmd_jump_edge( cmd, true );
		apply_landing_jump( base, *landing );
	}

}


