#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/settings.hpp>
#include <modules/visuals/skinchanger/custom_paint.h>

namespace features::changer {

	void agents::on_frame_stage_notify( )
	{
		if ( custom_paint::level_busy( ) )
			return;

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || systems::g_local.is_in_cinematic( ) || !local.pawn )
		{
			return;
		}

		const auto team = reinterpret_cast<C_BaseEntity*>(local.pawn)->m_iTeamNum();
		const auto selected_def_index = ( team == 3 ) ? settings::g_changer.agents.ct_def : ( team == 2 ) ? settings::g_changer.agents.t_def : static_cast< std::int16_t >( 0 );

		const econ_item_system::item_def* selected{ nullptr };
		if ( selected_def_index != 0 )
		{
			selected = g_econ_item_system.find_def( selected_def_index );
		}

		if ( this->m_tracked_pawn != local.pawn )
		{
			this->m_original_model.clear( );
			this->m_overridden = false;
			this->m_applied_handle = 0;
			this->m_applied_def = 0;
			this->m_tracked_team = 0;
			this->m_tracked_pawn = local.pawn;
		}

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>(local.pawn)->m_pGameSceneNode();
		if ( !memory::is_game_ptr( game_scene_node ) )
		{
			return;
		}

		const auto model_state = game_scene_node + SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash );

		if ( !selected || selected->model_player.empty( ) )
		{
			if ( this->m_overridden && !this->m_original_model.empty( ) )
			{
				memory::call<void>(PATTERN(PATTERN_SET_PLAYER_MODEL), local.pawn, this->m_original_model.c_str( ) );

				this->cycle_weapon_owners( local.pawn );

				this->m_applied_handle = 0;
				this->m_applied_def = 0;
				this->m_tracked_team = 0;
				this->m_overridden = false;
			}
			return;
		}

		if ( !this->m_overridden && this->m_original_model.empty( ) )
		{
			const auto model_name_ptr = reinterpret_cast<CModelState*>(model_state)->m_ModelName();
			if ( model_name_ptr )
			{
				this->m_original_model = memory::read_string( model_name_ptr );
			}
		}

		const auto current_handle = reinterpret_cast<CModelState*>(model_state)->m_hModel();
		const auto selection_matches = ( this->m_applied_def == selected->def_index );
		const auto team_matches = ( this->m_tracked_team == team );
		const auto handle_matches = ( this->m_applied_handle != 0 && current_handle == this->m_applied_handle );

		if ( this->m_overridden && selection_matches && team_matches && handle_matches )
		{
			return;
		}

		if ( this->m_overridden && !team_matches )
		{
			this->m_original_model.clear( );
			this->m_overridden = false;

			const auto model_name_ptr = reinterpret_cast<CModelState*>(model_state)->m_ModelName();
			if ( model_name_ptr )
			{
				this->m_original_model = memory::read_string( model_name_ptr );
			}
		}

		memory::call<void>(PATTERN(PATTERN_SET_PLAYER_MODEL), local.pawn, selected->model_player.c_str( ) );

		const auto collision = local.pawn + SCHEMA_OFFSET( "C_BaseModelEntity", "m_Collision"_hash );
		reinterpret_cast<CCollisionProperty*>(collision)->m_vecMins() = math::vector3( -16.0f, -16.0f, 0.0f );
		reinterpret_cast<CCollisionProperty*>(collision)->m_vecMaxs() = math::vector3( 16.0f, 16.0f, 72.0f );

		this->cycle_weapon_owners( local.pawn );

		this->m_applied_handle = reinterpret_cast<CModelState*>(model_state)->m_hModel();
		this->m_applied_def = selected->def_index;
		this->m_tracked_team = team;
		this->m_overridden = true;
	}

	void agents::cycle_weapon_owners( std::uintptr_t pawn )
	{
		const auto weapon_services = reinterpret_cast<C_BasePlayerPawn*>(pawn)->m_pWeaponServices();
		if ( !weapon_services )
		{
			return;
		}

		const auto weapons_base = weapon_services + SCHEMA_OFFSET( "CPlayer_WeaponServices", "m_hMyWeapons"_hash );
		const auto weapons_size = memory::read<int>( weapons_base );
		const auto weapons_data = memory::read<std::uintptr_t>( weapons_base + 0x8 );

		if ( !weapons_data || weapons_size <= 0 )
		{
			return;
		}

		for ( auto i = 0; i < weapons_size; ++i )
		{
			const auto handle = memory::read<std::uint32_t>( weapons_data + i * sizeof( std::uint32_t ) );
			const auto weapon = systems::g_entities.lookup( handle );

			if ( !weapon )
			{
				continue;
			}

			const auto saved_owner = reinterpret_cast<C_BaseEntity*>(weapon)->m_hOwnerEntity();
			if ( !saved_owner || saved_owner == 0xffffffff )
			{
				continue;
			}

			reinterpret_cast<C_BaseEntity*>(weapon)->m_hOwnerEntity() = saved_owner;
		}
	}

}



