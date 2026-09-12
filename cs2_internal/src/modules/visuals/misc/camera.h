#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>

#include "misc.hpp"
#include <core/common.hpp>

namespace features::misc {
	namespace {
		constexpr std::ptrdiff_t k_fov_offset{ 0x498 };
		constexpr std::ptrdiff_t k_aspect_ratio_offset{ 0x4d4 };
		constexpr std::ptrdiff_t k_view_flags_offset{ 0x551 };
		constexpr std::uint8_t k_explicit_aspect_ratio_flag{ 1u << 1 };

		[[nodiscard]] float normalize_yaw_delta( float delta )
		{
			while ( delta > 180.0f )
			{
				delta -= 360.0f;
			}

			while ( delta < -180.0f )
			{
				delta += 360.0f;
			}

			return delta;
		}

		[[nodiscard]] bool key_down( int vk )
		{
			return ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;
		}
	}

	bool camera::freecam_active( ) const
	{
		return settings::g_misc.m_camera.freecam.value && this->m_freecam_initialized;
	}

	void camera::reset_freecam( )
	{
		this->m_freecam_initialized = false;
		this->m_freecam_last_raw_angles = {};
	}

	void camera::sync_freecam_angles( ) const
	{
		if ( !this->m_freecam_initialized )
		{
			return;
		}

		systems::g_input.set_view_angles( this->m_freecam_angles );
	}

	bool camera::on_create_move( systems::input::usercmd* cmd )
	{
		const auto& cfg = settings::g_misc.m_camera;
		if ( !cfg.freecam.value )
		{
			this->reset_freecam( );
			return false;
		}

		if ( !cmd || !cmd->csgo_user_cmd.has_base( ) )
		{
			return false;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn )
		{
			return false;
		}

		const auto base = cmd->csgo_user_cmd.mutable_base( );
		if ( !base )
		{
			return false;
		}

		const auto raw_angles = systems::g_input.get_view_angles( );

		if ( !this->m_freecam_initialized )
		{
			const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( local.pawn )->m_pGameSceneNode( );
			if ( !game_scene_node )
			{
				return false;
			}

			const auto origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin( );
			const auto view_offset = reinterpret_cast<C_BaseModelEntity*>( local.pawn )->m_vecViewOffset( );
			this->m_freecam_origin = origin + view_offset;
			this->m_freecam_angles = raw_angles;
			this->m_freecam_last_raw_angles = raw_angles;
			this->m_freecam_initialized = true;
		}

		const auto dyaw = normalize_yaw_delta( raw_angles.y - this->m_freecam_last_raw_angles.y );
		const auto dpitch = raw_angles.x - this->m_freecam_last_raw_angles.x;

		this->m_freecam_angles.y += dyaw;
		this->m_freecam_angles.x += dpitch;
		this->m_freecam_angles.x = std::clamp( this->m_freecam_angles.x, -89.0f, 89.0f );
		this->m_freecam_angles.z = 0.0f;
		math::helpers::normalize_angles( this->m_freecam_angles );

		this->m_freecam_last_raw_angles = raw_angles;
		this->sync_freecam_angles( );

		auto frame_time = 0.015625f;
		if ( const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars ) )
		{
			frame_time = std::max( memory::read<float>( global_vars + 0x34 ), 0.001f );
		}

		math::vector3 forward{};
		math::vector3 left{};
		math::vector3 up{};
	math::helpers::angle_vectors_left( this->m_freecam_angles, &forward, &left, &up );

		auto speed = static_cast< float >( cfg.freecam_speed.value ) * frame_time;
		if ( key_down( VK_SHIFT ) )
		{
			speed *= 3.0f;
		}

		if ( key_down( 'W' ) )
		{
			this->m_freecam_origin += forward * speed;
		}

		if ( key_down( 'S' ) )
		{
			this->m_freecam_origin -= forward * speed;
		}

		if ( key_down( 'A' ) )
		{
			this->m_freecam_origin -= left * speed;
		}

		if ( key_down( 'D' ) )
		{
			this->m_freecam_origin += left * speed;
		}

		if ( key_down( VK_SPACE ) )
		{
			this->m_freecam_origin.z += speed;
		}

		if ( key_down( VK_CONTROL ) )
		{
			this->m_freecam_origin.z -= speed;
		}

		base->set_forwardmove( 0.0f );
		base->set_leftmove( 0.0f );
		if ( base->has_upmove( ) )
		{
			base->set_upmove( 0.0f );
		}

		constexpr auto move_mask =
			cstypes::command_buttons::in_forward |
			cstypes::command_buttons::in_back |
			cstypes::command_buttons::in_moveleft |
			cstypes::command_buttons::in_moveright |
			cstypes::command_buttons::in_jump |
			cstypes::command_buttons::in_duck |
			cstypes::command_buttons::in_sprint;

		cmd->buttons.value &= ~move_mask;
		cmd->buttons.value_changed &= ~move_mask;
		cmd->buttons.value_scroll &= ~move_mask;

		return true;
	}

	void camera::do_freecam( std::uintptr_t view_setup )
	{
		if ( !this->freecam_active( ) )
		{
			return;
		}

		memory::write<math::vector3>( view_setup + 0x4a0, this->m_freecam_origin );
		memory::write<math::vector3>( view_setup + 0x4b8, this->m_freecam_angles );
	}

	void camera::on_override_view( std::uintptr_t view_setup )
	{
		if ( this->freecam_active( ) )
		{
			this->do_freecam( view_setup );
			this->do_fov_change( view_setup, systems::g_local.get( ).pawn );
			this->do_aspect_ratio_change( view_setup );
			return;
		}

		const auto local = systems::g_local.get( );
		if ( local.is_alive && !systems::g_local.is_in_cinematic( ) && local.team >= 2 && local.pawn )
		{
			this->do_thirdperson( view_setup, local.pawn );
			this->do_fov_change( view_setup, local.pawn );
		}

		this->do_aspect_ratio_change( view_setup );
	}

	void camera::update_fov_sensitivity( std::uintptr_t player_pawn ) const
	{
		const auto& cfg = settings::g_misc.m_camera;
		const auto local = systems::g_local.get( );
		if ( player_pawn != local.pawn )
		{
			return;
		}

		if ( !memory::is_game_ptr( player_pawn ) || !systems::g_entities.is_cs_player_pawn( player_pawn ) )
		{
			return;
		}

		const auto is_scoped = reinterpret_cast<C_CSPlayerPawn*>(player_pawn)->m_bIsScoped();
		const auto want_scoped_fov = is_scoped && cfg.scoped_fov_override.value;
		if ( !cfg.change_fov.value && !want_scoped_fov )
		{
			return;
		}

		int zoom_level = 0;
		if ( is_scoped )
		{
			const auto ws = reinterpret_cast<C_BasePlayerPawn*>(player_pawn)->m_pWeaponServices( );
			if ( ws )
			{
				const auto wh = reinterpret_cast<CPlayer_WeaponServices*>(ws)->m_hActiveWeapon( );
				const auto wp = wh ? systems::g_entities.lookup( wh ) : 0;
				if ( wp )
					zoom_level = reinterpret_cast<C_CSWeaponBaseGun*>(wp)->m_zoomLevel( );
			}
		}
		const auto scoped_target = ( zoom_level >= 2 ) ? cfg.scoped_fov_stage2.value : cfg.scoped_fov_stage1.value;
		const auto target_fov = want_scoped_fov ? scoped_target : cfg.fov.value;

		if ( is_scoped == this->m_cached_scoped && target_fov == this->m_cached_target_fov && this->m_cached_fov_sensitivity >= 0.0f )
		{
			const auto current_adjust = reinterpret_cast<C_BasePlayerPawn*>(player_pawn)->m_flFOVSensitivityAdjust();
			if ( std::fabsf( current_adjust - this->m_cached_fov_sensitivity ) < 0.0001f )
			{
				return;
			}
		}

		this->m_cached_scoped = is_scoped;
		this->m_cached_target_fov = target_fov;

		const auto ratio = CONVAR ("zoom_sensitivity_ratio")->get<float>( );
		const auto desired = ratio * ( target_fov / 90.0f );

		this->m_cached_fov_sensitivity = desired;
		reinterpret_cast<C_BasePlayerPawn*>(player_pawn)->m_flFOVSensitivityAdjust() = desired;
	}

	void camera::do_thirdperson( std::uintptr_t view_setup, std::uintptr_t local_pawn ) const
	{
		const auto& cfg = settings::g_misc.m_camera;
		if ( !cfg.thirdperson.value )
		{
			return;
		}

		if ( !systems::g_entities.is_cs_player_pawn( local_pawn ) )
		{
			return;
		}

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>(local_pawn)->m_pGameSceneNode();
		if ( !memory::is_game_ptr( game_scene_node ) )
		{
			return;
		}

		const auto origin = reinterpret_cast<CGameSceneNode*>(game_scene_node)->m_vecAbsOrigin();
		const auto view_offset = reinterpret_cast<C_BaseModelEntity*>(local_pawn)->m_vecViewOffset();
		const auto eye_position = origin + view_offset;
		const auto view_angles = systems::g_input.get_view_angles( );

		math::vector3 forward{};
		{
			math::helpers::angle_vectors_left( view_angles, &forward );
		}

		auto camera_position = eye_position - forward * cfg.thirdperson_distance;
		auto hull_size = cfg.thirdperson_hull_size.value;
		if ( !std::isfinite( hull_size ) || hull_size <= 0.0f )
		{
			hull_size = 5.0f;
		}

		const auto mins = math::vector3{ -hull_size, -hull_size, -hull_size };
		const auto maxs = math::vector3{ hull_size, hull_size, hull_size };
		const auto result = systems::g_tracing.trace_hull( eye_position, camera_position, mins, maxs, local_pawn );
		if ( result.all_solid )
		{
			camera_position = eye_position;
		}
		else if ( result.fraction < 1.0f )
		{
			camera_position = eye_position + ( camera_position - eye_position ) * result.fraction;
		}

		memory::write<math::vector3>( view_setup + 0x4a0, camera_position );
	}

	void camera::do_fov_change( std::uintptr_t view_setup, std::uintptr_t local_pawn ) const
	{
		const auto& cfg = settings::g_misc.m_camera;
		if ( !local_pawn || !systems::g_entities.is_cs_player_pawn( local_pawn ) )
		{
			return;
		}

		const auto is_scoped = reinterpret_cast<C_CSPlayerPawn*>(local_pawn)->m_bIsScoped();
		const auto want_scoped_fov = is_scoped && cfg.scoped_fov_override.value;
		if ( !cfg.change_fov.value && !want_scoped_fov )
		{
			return;
		}

		int zoom_level = 0;
		if ( is_scoped )
		{
			const auto ws = reinterpret_cast<C_BasePlayerPawn*>(local_pawn)->m_pWeaponServices( );
			if ( ws )
			{
				const auto wh = reinterpret_cast<CPlayer_WeaponServices*>(ws)->m_hActiveWeapon( );
				const auto wp = wh ? systems::g_entities.lookup( wh ) : 0;
				if ( wp )
					zoom_level = reinterpret_cast<C_CSWeaponBaseGun*>(wp)->m_zoomLevel( );
			}
		}
		const auto scoped_target = ( zoom_level >= 2 ) ? cfg.scoped_fov_stage2.value : cfg.scoped_fov_stage1.value;
		const auto target_fov = want_scoped_fov ? scoped_target : cfg.fov.value;

		if ( target_fov < 30.0f || target_fov > 179.0f )
		{
			return;
		}

		memory::write<float>( view_setup + k_fov_offset, target_fov );

		this->update_fov_sensitivity( local_pawn );
	}

	void camera::do_aspect_ratio_change( std::uintptr_t view_setup )
	{
		if ( !view_setup )
		{
			return;
		}

		const auto& cfg = settings::g_misc.m_camera;
		if ( !cfg.change_aspect_ratio.value )
			return;

		const float ar = cfg.aspect_ratio.value;
		if ( !std::isfinite( ar ) || ar < 0.5f || ar > 3.5f )
			return;

		const auto flags = memory::read<std::uint8_t>( view_setup + k_view_flags_offset );
		memory::write<float>( view_setup + k_aspect_ratio_offset, ar );
		memory::write<std::uint8_t>( view_setup + k_view_flags_offset, flags | k_explicit_aspect_ratio_flag );
	}

}



