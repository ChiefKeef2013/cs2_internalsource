#pragma once

#include <core/common.hpp>
#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/common.hpp>

#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerController.h>
#include <valve/schemas/CBasePlayerPawn.h>
#include <valve/schemas/CCollisionProperty.h>
#include <valve/schemas/CCSGameRules.h>
#include <valve/schemas/CEconEntity.h>
#include <valve/schemas/CGameSceneNode.h>
#include <valve/schemas/CGrenade.h>
#include <valve/schemas/CPlantedC4.h>
#include <valve/schemas/CWeapon.h>

namespace systems {

	void input::update( )
	{
		const auto local_controller = memory::read<std::uintptr_t>( addresses::globals::local_player_controller );
		if ( !local_controller )
		{
			this->m_current_cmd = nullptr;
			return;
		}

		this->m_current_cmd = this->get_current_cmd( local_controller );
	}

	void input::apply( )
	{
		const auto local = systems::g_local.get ();

		if ( !this->m_current_cmd ) {
			return;
		}

		const auto base = this->m_current_cmd->csgo_user_cmd.mutable_base( );
		if ( !base ) {
			return;
		}

		auto has_move_subticks = [] (proto::base_usercmd_pb* base_cmd) {

			for (size_t i = 0; i < base_cmd->subtick_moves_size (); i++) {
				proto::subtick_move_step* step = base_cmd->mutable_subtick_moves (i);
				if (step->m_has_bits.test (0x8) || step->m_has_bits.test (0x10))
					return true;
			}

			return false;
		};

		if (!has_move_subticks (base)) {
			if (const auto step = systems::g_input.acquire_subtick_step (base->mutable_subtick_moves ())) {

				const auto movement_services = local.pawn ? reinterpret_cast<C_BasePlayerPawn*>( local.pawn )->m_pMovementServices() : 0;
				if (movement_services) {
					step->set_button (0);
					step->set_pressed (false);
					step->set_when (0.0f);
					step->set_analog_forward_delta (base->forwardmove () - reinterpret_cast<CPlayer_MovementServices*>( movement_services )->m_flCmdForwardMove());
					step->set_analog_left_delta (base->leftmove () - reinterpret_cast<CPlayer_MovementServices*>( movement_services )->m_flCmdLeftMove());
				}
			}
		}

		auto buttons = const_cast<proto::in_button_state_pb*>( base->buttons_pb( ) );
		if ( !buttons )
		{
			const auto raw_base =
				reinterpret_cast<std::uintptr_t>( base ) - proto::message_impl_offset;
			const auto arena_bits = memory::read<std::uintptr_t>( raw_base + 0x08 );
			auto arena = arena_bits & ~0x3ull;
			if ( arena_bits & 1 )
			{
				arena = memory::read<std::uintptr_t>( arena );
			}

			const auto raw_buttons =
				memory::call<void*>( PATTERN(PATTERN_BUTTON_STATE_ALLOC), arena );
			if ( raw_buttons )
			{
				base->m_buttons_pb =
					reinterpret_cast<proto::in_button_state_pb*>( raw_buttons );
				buttons = proto::impl_ptr<proto::in_button_state_pb>( raw_buttons );
			}
		}

		if ( buttons )
		{
			base->m_has_bits.set( 0x2u );
			buttons->set_buttonstate1( this->m_current_cmd->buttons.value );
			buttons->set_buttonstate2( this->m_current_cmd->buttons.value_changed );
			buttons->set_buttonstate3( this->m_current_cmd->buttons.value_scroll );
		}

		this->calculate_crc( base );
	}

	input::usercmd* input::get_current_cmd( std::uintptr_t local_controller ) const
	{
		const auto get_usercmd_base = PATTERN(PATTERN_GET_USERCMD_BASE);
		const auto get_usercmd = PATTERN(PATTERN_GET_USERCMD);
		if ( !get_usercmd_base || !get_usercmd )
		{
			return nullptr;
		}

		const auto usercmd_base = memory::call<std::uintptr_t>( get_usercmd_base, local_controller );
		if ( !usercmd_base )
		{
			return nullptr;
		}

		const auto sequence = memory::read<int>( usercmd_base + 0x5910 );
		return memory::call<usercmd*>( get_usercmd, local_controller, sequence );
	}

	proto::subtick_move_step* input::acquire_subtick_step( proto::repeated_ptr_field<proto::subtick_move_step>* subtick_moves ) const
	{
		if ( !subtick_moves )
		{
			return nullptr;
		}

		if ( subtick_moves->m_rep )
		{
			if ( subtick_moves->m_current_size < subtick_moves->m_rep->allocated_size )
			{
				const auto element = subtick_moves->m_rep->elements[ subtick_moves->m_current_size ];
				if ( element )
				{
					subtick_moves->m_current_size++;
					const auto step = proto::impl_ptr<proto::subtick_move_step>( element );
					*step = {};
					return step;
				}
			}
		}

		const auto move_step = memory::call<void*>(PATTERN(PATTERN_SUBTICK_MOVE_ALLOC), subtick_moves->m_arena );
		if ( move_step )
		{
			memory::call<std::uintptr_t>(PATTERN(PATTERN_UTL_VECTOR_PUSH), reinterpret_cast< std::uintptr_t >( subtick_moves ), reinterpret_cast< std::uintptr_t >( move_step ) );
			return proto::impl_ptr<proto::subtick_move_step>( move_step );
		}

		return nullptr;
	}

	math::vector3 input::get_view_angles( ) const
	{
		return *memory::call<math::vector3*>(PATTERN(PATTERN_GET_VIEW_ANGLES), addresses::globals::csgo_input, 0 );
	}

	proto::input_history_entry* input::push_input_history( usercmd* cmd, const input_history_params& params ) const
	{
		auto history_field = cmd->csgo_user_cmd.mutable_input_history( );
		if ( !history_field )
		{
			return nullptr;
		}

		proto::input_history_entry* entry{ nullptr };

		if ( history_field->m_rep )
		{
			if ( history_field->m_current_size < history_field->m_rep->allocated_size )
			{
				auto element = history_field->m_rep->elements[ history_field->m_current_size ];
				if ( element )
				{
					history_field->m_current_size++;
					entry = proto::impl_ptr<proto::input_history_entry>( element );
				}
			}
		}

		if ( !entry )
		{
			auto raw = memory::call<void*>(PATTERN(PATTERN_HISTORY_FIELD_ALLOC), history_field->m_arena );
			if ( !raw )
			{
				return nullptr;
			}

			memory::call<std::uintptr_t>(PATTERN(PATTERN_UTL_VECTOR_PUSH), reinterpret_cast< std::uintptr_t >( history_field ), reinterpret_cast< std::uintptr_t >( raw ) );

			entry = proto::impl_ptr<proto::input_history_entry>( raw );
		}

		if ( !entry )
		{
			return nullptr;
		}

		entry->set_render_tick_count( params.render_tick );
		entry->set_render_tick_fraction( params.render_frac );
		entry->set_player_tick_count( params.player_tick );
		entry->set_player_tick_fraction( params.player_frac );
		entry->set_frame_number( params.frame_number );
		entry->set_target_ent_index( params.target_ent_index );

		if ( auto va = entry->mutable_view_angles( ) )
		{
			va->set_x( params.view_angles.x );
			va->set_y( params.view_angles.y );
			va->set_z( params.view_angles.z );
		}

		if ( entry->has_shoot_position( ) )
		{
			if ( auto sp = entry->mutable_shoot_position( ) )
			{
				sp->set_x( params.shoot_position.x );
				sp->set_y( params.shoot_position.y );
				sp->set_z( params.shoot_position.z );
			}
		}

		if ( auto ci = entry->mutable_cl_interp( ) )
		{
			ci->set_frac( params.cl_interp_frac );
		}

		if ( auto si0 = entry->mutable_sv_interp0( ) )
		{
			si0->set_src_tick( params.sv_interp0_src );
			si0->set_dst_tick( params.sv_interp0_dst );
			si0->set_frac( params.sv_interp0_frac );
		}

		if ( auto si1 = entry->mutable_sv_interp1( ) )
		{
			si1->set_src_tick( params.sv_interp1_src );
			si1->set_dst_tick( params.sv_interp1_dst );
			si1->set_frac( params.sv_interp1_frac );
		}

		if ( auto pi = entry->mutable_player_interp( ) )
		{
			pi->set_src_tick( params.player_interp_src );
			pi->set_dst_tick( params.player_interp_dst );
			pi->set_frac( params.player_interp_frac );
		}

		if ( params.fill_cheat_check_data )
		{
			if ( auto head = entry->mutable_target_head_pos_check( ) )
			{
				head->set_x( params.target_head_pos.x );
				head->set_y( params.target_head_pos.y );
				head->set_z( params.target_head_pos.z );
			}

			if ( auto abs_pos = entry->mutable_target_abs_pos_check( ) )
			{
				abs_pos->set_x( params.target_abs_pos.x );
				abs_pos->set_y( params.target_abs_pos.y );
				abs_pos->set_z( params.target_abs_pos.z );
			}

			if ( auto abs_ang = entry->mutable_target_abs_ang_check( ) )
			{
				abs_ang->set_x( params.target_abs_ang.x );
				abs_ang->set_y( params.target_abs_ang.y );
				abs_ang->set_z( params.target_abs_ang.z );
			}
		}

		return entry;
	}

	void input::set_view_angles( const math::vector3& angles ) const
	{
		memory::call<void>(PATTERN(PATTERN_SET_VIEW_ANGLES), addresses::globals::csgo_input, 0, &angles );
	}

	void input::desubtick( usercmd* cmd ) const
	{
		cmd->csgo_user_cmd.mutable_base( )->mutable_subtick_moves( )->clear( );
	}

	void input::neutralize( usercmd* cmd ) const
	{
		if ( !cmd )
		{
			return;
		}

		cmd->buttons.value = 0;
		cmd->buttons.value_changed = 0;
		cmd->buttons.value_scroll = 0;

		if ( !cmd->csgo_user_cmd.has_base( ) )
		{
			return;
		}

		const auto base = cmd->csgo_user_cmd.mutable_base( );
		if ( !base )
		{
			return;
		}

		base->set_forwardmove( 0.0f );
		base->set_leftmove( 0.0f );

		if ( auto* buttons = const_cast< proto::in_button_state_pb* >( base->buttons_pb( ) ) )
		{
			buttons->set_buttonstate1( 0 );
			buttons->set_buttonstate2( 0 );
			buttons->set_buttonstate3( 0 );
		}

		if ( auto* steps = base->mutable_subtick_moves( ) )
		{
			steps->clear( );
		}
	}

	void input::set_weapon_select( usercmd* cmd, std::uintptr_t csgo_input ) const
	{
		const auto frame_data = csgo_input + 552;
		const auto weapon_handle = memory::read<uint32_t>( frame_data + 1132 );

		if ( !weapon_handle )
		{
			return;
		}

		const auto weapon = systems::g_entities.lookup( weapon_handle );
		if ( !weapon )
		{
			return;
		}

		auto entity_index{ -1 };
		memory::call<int*>(PATTERN(PATTERN_WEAPON_GET_ENTITY_INDEX), weapon, &entity_index );

		if ( entity_index == -1 )
		{
			return;
		}

		const auto base = cmd->csgo_user_cmd.mutable_base( );
		if ( base )
		{
			base->set_weaponselect( entity_index );
		}
	}

	input::usercmd* input::get_command_by_sequence( std::uintptr_t local_controller, int sequence ) const
	{
		const auto get_usercmd_base = PATTERN(PATTERN_GET_USERCMD_BASE);
		if ( !get_usercmd_base )
		{
			return nullptr;
		}

		const auto usercmd_base = memory::call<std::uintptr_t>( get_usercmd_base, local_controller );
		if ( !usercmd_base )
		{
			return nullptr;
		}

		const auto index = sequence % 150;
		return reinterpret_cast< usercmd* >( usercmd_base + sizeof( usercmd ) * index );
	}

	bool input::is_subtick_overwrite( usercmd* cmd ) const
	{
		return memory::read<int>( reinterpret_cast< std::uintptr_t >( cmd ) + 148 ) == 2;
	}

	bool input::calculate_crc( proto::base_usercmd_pb* base ) const
	{
		if ( !base )
		{
			return false;
		}

		const auto string_copy = PATTERN(PATTERN_STRING_COPY);
		const auto serialize_move_crc = PATTERN(PATTERN_SERIALIZE_MOVE_CRC);
		if ( !string_copy || !serialize_move_crc )
		{
			return false;
		}

		const auto btns = base->buttons_pb( );
		const auto va = base->viewangles( );

		std::uint8_t buf[ 64 ]{};
		std::uint8_t btn_size{ 0 };

		auto p = buf;

		auto bs1 = btns ? btns->buttonstate1( ) : 0;
		auto bs2 = btns ? btns->buttonstate2( ) : 0;
		auto bs3 = btns ? btns->buttonstate3( ) : 0;

		if ( bs1 ) { btn_size += 9; }
		if ( bs2 ) { btn_size += 9; }
		if ( bs3 ) { btn_size += 9; }

		if ( btn_size > 0 )
		{
			*p++ = 0x1a;
			*p++ = btn_size;

			if ( bs1 ) { *p++ = 0x09; std::memcpy( p, &bs1, 8 ); p += 8; }
			if ( bs2 ) { *p++ = 0x11; std::memcpy( p, &bs2, 8 ); p += 8; }
			if ( bs3 ) { *p++ = 0x19; std::memcpy( p, &bs3, 8 ); p += 8; }
		}

		auto pitch = va ? va->x( ) : 0.0f;
		auto yaw = va ? va->y( ) : 0.0f;
		auto roll = va ? va->z( ) : 0.0f;

		std::uint8_t va_size{ 0 };
		if ( pitch != 0.0f ) { va_size += 5; }
		if ( yaw != 0.0f ) { va_size += 5; }
		if ( roll != 0.0f ) { va_size += 5; }

		if ( va_size > 0 )
		{
			*p++ = 0x22;
			*p++ = va_size;

			if ( pitch != 0.0f ) { *p++ = 0x0d; std::memcpy( p, &pitch, 4 ); p += 4; }
			if ( yaw != 0.0f ) { *p++ = 0x15; std::memcpy( p, &yaw, 4 ); p += 4; }
			if ( roll != 0.0f ) { *p++ = 0x1d; std::memcpy( p, &roll, 4 ); p += 4; }
		}

		auto total = static_cast< int >( p - buf );

		base->m_has_bits.set( 0x1u );

		auto raw_msg = reinterpret_cast< std::uintptr_t >( base ) - proto::message_impl_offset;
		auto arena_raw = memory::read<std::uintptr_t>( raw_msg + 0x08 );
		auto arena = arena_raw & ~0x3ull;

		if ( arena_raw & 1 )
		{
			arena = memory::read<std::uintptr_t>( arena );
		}

		std::uint8_t msg[ 0x18 ]{};
		memory::call<void>( string_copy, reinterpret_cast< std::uintptr_t >( msg ), reinterpret_cast< std::uintptr_t >( buf ), total );
		memory::call<void>( serialize_move_crc, &base->m_move_crc, reinterpret_cast< std::uintptr_t >( msg ), arena );

		return true;
	}

}





