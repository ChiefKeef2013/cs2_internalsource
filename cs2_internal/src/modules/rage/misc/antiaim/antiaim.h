#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/common.hpp>
#include <core/menu/rendering.hpp>
#include <imgui/imgui.h>

namespace features::combat {

	void misc::antiaim::on_create_move( systems::input::usercmd* cmd )
	{
		this->m_antiaim_active = false;

		if ( !settings::g_combat.m_antiaim.enabled.value )
		{
			return;
		}

		if ( systems::g_local.is_in_cinematic( ) || systems::g_local.is_in_time_freeze( ) )
		{
			return;
		}

		if ( settings::g_combat.m_antiaim.manual_left.value && settings::g_combat.m_antiaim.manual_right.value )
		{
			settings::g_combat.m_antiaim.manual_right.value = false;
			settings::g_combat.m_antiaim.manual_right.bind.active = false;
		}

		if ( settings::g_combat.m_antiaim.manual_left.value )
		{
			this->m_yaw_side = -1;
		}
		else if ( settings::g_combat.m_antiaim.manual_right.value )
		{
			this->m_yaw_side = 1;
		}
		else
		{
			this->m_yaw_side = 0;
		}

		const auto local = systems::g_local.get( );
		const auto base = cmd->csgo_user_cmd.mutable_base( );
		const auto view_angles = systems::g_input.get_view_angles( );
		const auto& ctx = g_shared.ctx( );

		if ( cmd->buttons.value & cstypes::command_buttons::in_use )
		{
			return;
		}

		if ( ctx.weapon_type == cstypes::weapon_type::grenade )
		{
			if ( reinterpret_cast<C_BaseCSGrenade*>(ctx.weapon)->m_fThrowTime() > 0.0f )
			{
				return;
			}
		}

		const auto move_type = reinterpret_cast<C_BaseEntity*>(local.pawn)->m_nActualMoveType();
		if ( move_type == cstypes::move_type::ladder || move_type == cstypes::move_type::noclip )
		{
			return;
		}

		if ( this->is_near_ladder( local.pawn ) )
		{
			return;
		}

		this->m_old_angles = view_angles;
		this->m_modified_angles = this->m_old_angles;
		this->m_modified_angles.x = this->get_pitch( this->m_old_angles.x );
		this->m_modified_angles.y = this->get_yaw( this->m_old_angles, local );

	math::helpers::normalize_angles( this->m_modified_angles );

		if ( !base )
		{
			return;
		}

		auto* viewangles = base->mutable_viewangles( );
		if ( !viewangles )
		{
			return;
		}

		viewangles->set_x( this->m_modified_angles.x );
		viewangles->set_y( this->m_modified_angles.y );
		viewangles->set_z( this->m_modified_angles.z );

		this->m_antiaim_active = true;
		this->m_should_correct = true;

		this->correct_movement( cmd );
	}

	void misc::antiaim::on_render( xdraw::draw_list& draw_list )
	{
		const auto view_yaw = systems::g_input.get_view_angles( ).y;
		this->update_mouse_override( view_yaw );
		this->draw_mouse_override_indicator( draw_list );

		if ( !settings::g_combat.m_antiaim.enabled.value || !settings::g_combat.m_antiaim.direction_indicator.value )
		{
			return;
		}

		if ( !this->m_antiaim_active )
		{
			return;
		}

		if ( !systems::g_frame_data.valid( ) )
		{
			return;
		}

		const auto origin = systems::g_frame_data.origin( );
		const auto yaw = this->m_indicator_yaw * ( std::numbers::pi_v<float> / 180.0f );

		constexpr auto ring_radius{ 30.0f };
		constexpr auto feet_offset{ -1.5f };
		constexpr auto ring_segments{ 48 };
		constexpr auto accent_segments{ 16 };
		constexpr auto accent_half{ 0.42f };

		const auto& cfg = settings::g_combat.m_antiaim;
		const auto& color = cfg.direction_indicator_color;
		const auto base = math::vector3{ origin.x, origin.y, origin.z + feet_offset };

		auto world_on_ring = [ & ]( float angle, float radius ) -> math::vector3
			{
				return math::vector3{
					base.x + std::cosf( angle ) * radius,
					base.y + std::sinf( angle ) * radius,
					base.z
				};
			};

		auto project = [ ]( const math::vector3& world, math::vector2& out ) -> bool
			{
				const auto sp = systems::g_view.project( world );
				if ( !systems::g_view.projection_valid( sp ) )
				{
					return false;
				}
				out = { sp.x, sp.y };
				return true;
			};

		math::vector2 ring[ ring_segments ];
		float ring_pts[ ring_segments * 2 ];
		for ( auto i = 0; i < ring_segments; ++i )
		{
			const auto angle = ( static_cast< float >( i ) / static_cast< float >( ring_segments ) ) * ( std::numbers::pi_v<float> * 2.0f );
			if ( !project( world_on_ring( angle, ring_radius ), ring[ i ] ) )
			{
				return;
			}
			ring_pts[ i * 2 ] = ring[ i ].x;
			ring_pts[ i * 2 + 1 ] = ring[ i ].y;
		}

		const auto ring_col = xdraw::color{ color.value.r, color.value.g, color.value.b, static_cast< std::uint8_t >( color.value.a * 0.28f ) };
		draw_list.polyline( ring_pts, ring_col, true, 1.15f );

		const auto tick_len = 3.6f;
		for ( auto t = 0; t < 4; ++t )
		{
			const auto angle = yaw + static_cast< float >( t ) * ( std::numbers::pi_v<float> * 0.5f );
			math::vector2 inner{}, outer{};
			if ( !project( world_on_ring( angle, ring_radius - tick_len ), inner ) ||
				!project( world_on_ring( angle, ring_radius + tick_len ), outer ) )
			{
				continue;
			}

			const auto tick_a = ( t == 0 ) ? static_cast< std::uint8_t >( color.value.a * 0.85f ) : static_cast< std::uint8_t >( color.value.a * 0.40f );
			draw_list.line( inner.x, inner.y, outer.x, outer.y, { color.value.r, color.value.g, color.value.b, tick_a }, t == 0 ? 1.8f : 1.15f );
		}

		math::vector2 accent[ accent_segments + 1 ];
		float accent_pts[ ( accent_segments + 1 ) * 2 ];
		xdraw::color accent_cols[ accent_segments + 1 ];
		auto accent_ok{ true };
		for ( auto i = 0; i <= accent_segments; ++i )
		{
			const auto frac = static_cast< float >( i ) / static_cast< float >( accent_segments );
			const auto angle = yaw + ( frac - 0.5f ) * 2.0f * accent_half;
			if ( !project( world_on_ring( angle, ring_radius ), accent[ i ] ) )
			{
				accent_ok = false;
				break;
			}

			const auto edge = 1.0f - std::fabsf( frac - 0.5f ) * 2.0f;
			const auto fade = edge * edge * ( 3.0f - 2.0f * edge );
			accent_pts[ i * 2 ] = accent[ i ].x;
			accent_pts[ i * 2 + 1 ] = accent[ i ].y;
			accent_cols[ i ] = { color.value.r, color.value.g, color.value.b, static_cast< std::uint8_t >( color.value.a * ( 0.20f + fade * 0.80f ) ) };
		}

		if ( accent_ok )
		{
			if ( cfg.direction_indicator_glow )
			{
				auto& glow = xdraw::get_glow( );
				xdraw::color glow_cols[ accent_segments + 1 ];
				for ( auto i = 0; i <= accent_segments; ++i )
				{
					const auto a = static_cast< std::uint8_t >( static_cast< float >( accent_cols[ i ].a ) * cfg.direction_indicator_glow_strength );
					glow_cols[ i ] = { color.value.r, color.value.g, color.value.b, a };
				}
				glow.polyline_gradient( std::span<const float>{ accent_pts, ( accent_segments + 1 ) * 2 }, glow_cols, false, 4.5f );
			}

			draw_list.polyline_gradient( std::span<const float>{ accent_pts, ( accent_segments + 1 ) * 2 }, accent_cols, false, 2.4f );
		}

		math::vector2 tip{}, left{}, right{}, notch{};
		if ( !project( world_on_ring( yaw, ring_radius + 12.0f ), tip ) ||
			!project( world_on_ring( yaw + 0.28f, ring_radius + 0.4f ), left ) ||
			!project( world_on_ring( yaw - 0.28f, ring_radius + 0.4f ), right ) ||
			!project( world_on_ring( yaw, ring_radius + 2.4f ), notch ) )
		{
			return;
		}

		const auto fill = xdraw::color{ color.value.r, color.value.g, color.value.b, static_cast< std::uint8_t >( color.value.a * 0.92f ) };
		const auto fill_dim = xdraw::color{ color.value.r, color.value.g, color.value.b, static_cast< std::uint8_t >( color.value.a * 0.55f ) };
		auto& overlay = xdraw::get( xdraw::layer::top );

		if ( cfg.direction_indicator_glow )
		{
			auto& glow = xdraw::get_glow( xdraw::layer::top );
			const auto ga = static_cast< std::uint8_t >( static_cast< float >( color.value.a ) * cfg.direction_indicator_glow_strength );
			const auto glow_col = xdraw::color{ color.value.r, color.value.g, color.value.b, ga };
			glow.triangle_filled( tip.x, tip.y, left.x, left.y, notch.x, notch.y, glow_col );
			glow.triangle_filled( tip.x, tip.y, notch.x, notch.y, right.x, right.y, glow_col );
		}

		overlay.triangle_filled( tip.x, tip.y, left.x, left.y, notch.x, notch.y, fill );
		overlay.triangle_filled( tip.x, tip.y, notch.x, notch.y, right.x, right.y, fill_dim );
		overlay.line( left.x, left.y, tip.x, tip.y, fill, 1.25f );
		overlay.line( right.x, right.y, tip.x, tip.y, fill, 1.25f );
	}

	float misc::antiaim::get_pitch( float view_pitch )
	{
		float pitch{};

		switch ( settings::g_combat.m_antiaim.pitch )
		{
		case settings::combat::antiaim::pitch_mode::down:
			pitch = 89.0f;
			break;
		case settings::combat::antiaim::pitch_mode::up:
			pitch = -89.0f;
			break;
		default:
			pitch = view_pitch;
			break;
		}

		if ( settings::g_combat.m_antiaim.pitch_jitter.value )
		{
			this->m_pitch_jitter_flip = !this->m_pitch_jitter_flip;
			const auto amount = static_cast< float >( settings::g_combat.m_antiaim.pitch_jitter_amount.value );
			pitch += this->m_pitch_jitter_flip ? amount : -amount;
		}

		return pitch;
	}

	float misc::antiaim::get_yaw( const math::vector3& view_angles, const systems::local::snapshot& local )
	{
		this->update_mouse_override( view_angles.y );
		if ( ( this->m_mouse_override_active || this->m_mouse_override_latched ) && !this->m_mouse_override_centered )
		{
			this->m_indicator_yaw = this->m_mouse_override_yaw;
			return this->m_mouse_override_yaw;
		}

		auto base_yaw_offset{ 180.0f };

		const auto view_yaw = view_angles.y;
		auto base_yaw = view_yaw - base_yaw_offset;

		const auto local_game_scene_node = reinterpret_cast<C_BaseEntity*>(local.pawn)->m_pGameSceneNode();
		if ( !local_game_scene_node )
		{
			return base_yaw;
		}

		const auto local_origin = reinterpret_cast<CGameSceneNode*>(local_game_scene_node)->m_vecAbsOrigin();
		const auto players = systems::g_entities.get_by_type( systems::entities::type::player );
		const auto eye_pos = local_origin + reinterpret_cast<C_BaseModelEntity*>(local.pawn)->m_vecViewOffset();

		if ( settings::g_combat.m_antiaim.avoid_backstab.value )
		{
			constexpr auto backstab_range_sq = 350.0f * 350.0f;
			auto knife_dist = std::numeric_limits<float>::max( );
			auto knife_yaw{ 0.0f };
			auto knife_found{ false };

			for ( const auto& p : players )
			{
				if ( !p.ptr || p.ptr == local.controller )
				{
					continue;
				}

				if ( !reinterpret_cast<CCSPlayerController*>(p.ptr)->m_bPawnIsAlive() )
				{
					continue;
				}

				const auto pawn = systems::g_entities.player_pawn( p.ptr );

				if ( !pawn || pawn == local.pawn )
				{
					continue;
				}

				const auto team = reinterpret_cast<C_BaseEntity*>(pawn)->m_iTeamNum();
				if ( !local.is_this_other_team( team ) )
				{
					continue;
				}

				const auto health = reinterpret_cast<C_BaseEntity*>(pawn)->m_iHealth();
				if ( health <= 0 )
				{
					continue;
				}

				const auto enemy_game_scene_node = reinterpret_cast<C_BaseEntity*>(pawn)->m_pGameSceneNode();
				if ( !enemy_game_scene_node )
				{
					continue;
				}

				const auto enemy_origin = reinterpret_cast<CGameSceneNode*>(enemy_game_scene_node)->m_vecAbsOrigin();
				const auto dx = enemy_origin.x - local_origin.x;
				const auto dy = enemy_origin.y - local_origin.y;
				const auto dist_sq = dx * dx + dy * dy;

				if ( dist_sq > backstab_range_sq )
				{
					continue;
				}

				const auto weapon_services = reinterpret_cast<C_BasePlayerPawn*>(pawn)->m_pWeaponServices();
				if ( !weapon_services )
				{
					continue;
				}

				const auto weapon_handle = reinterpret_cast<CPlayer_WeaponServices*>(weapon_services)->m_hActiveWeapon();
				if ( !weapon_handle )
				{
					continue;
				}

				const auto weapon = systems::g_entities.lookup( weapon_handle );
				if ( !weapon )
				{
					continue;
				}

				const auto weapon_vdata = memory::read<std::uintptr_t>( weapon + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
				if ( !weapon_vdata )
				{
					continue;
				}

				const auto weapon_type = reinterpret_cast<CCSWeaponBaseVData*>(weapon_vdata)->m_WeaponType();
				if ( weapon_type != cstypes::weapon_type::knife )
				{
					continue;
				}

				if ( dist_sq < knife_dist )
				{
					knife_dist = dist_sq;
					knife_yaw = std::atan2f( dy, dx ) * ( 180.0f / std::numbers::pi_v<float> );
					knife_found = true;
				}
			}

			if ( knife_found )
			{
				this->m_indicator_yaw = knife_yaw;
				return knife_yaw;
			}
		}

		const auto pick_target_yaw = [ & ]( ) -> std::optional<float>
			{
				auto best_yaw = base_yaw;
				auto best_threat_score = std::numeric_limits<float>::max( );

				for ( const auto& p : players )
				{
					if ( !p.ptr || p.ptr == local.controller )
					{
						continue;
					}

					if ( !reinterpret_cast<CCSPlayerController*>(p.ptr)->m_bPawnIsAlive() )
					{
						continue;
					}

					const auto pawn = systems::g_entities.player_pawn( p.ptr );

					if ( !pawn || pawn == local.pawn )
					{
						continue;
					}

					const auto team = reinterpret_cast<C_BaseEntity*>(pawn)->m_iTeamNum();
					if ( !local.is_this_other_team( team ) )
					{
						continue;
					}

					if ( reinterpret_cast<C_BaseEntity*>(pawn)->m_iHealth() <= 0 )
					{
						continue;
					}

					if ( reinterpret_cast<C_CSPlayerPawn*>(pawn)->m_bGunGameImmunity() )
					{
						continue;
					}

					const auto enemy_game_scene_node = reinterpret_cast<C_BaseEntity*>(pawn)->m_pGameSceneNode();
					if ( !enemy_game_scene_node )
					{
						continue;
					}

					const auto enemy_origin = reinterpret_cast<CGameSceneNode*>(enemy_game_scene_node)->m_vecAbsOrigin();
					const auto enemy_eye_pos = enemy_origin + reinterpret_cast<C_BaseModelEntity*>(pawn)->m_vecViewOffset();
					const auto angle_to_enemy = math::helpers::calculate_angle( eye_pos, enemy_eye_pos );
					const auto fov = math::helpers::angle_distance( view_angles, angle_to_enemy );
					const auto distance = eye_pos.distance( enemy_eye_pos );

					auto threat_score = fov * 4.0f + distance * 0.01f;

					math::vector3 enemy_forward{};
					const auto enemy_eye_angles = reinterpret_cast<C_CSPlayerPawn*>(pawn)->m_angEyeAngles();
				math::helpers::angle_vectors_left( enemy_eye_angles, &enemy_forward );
					const auto direction_to_us = ( eye_pos - enemy_eye_pos ).normalized( );
					threat_score -= std::clamp( enemy_forward.dot( direction_to_us ), -1.0f, 1.0f ) * 25.0f;

					if ( systems::g_tracing.is_visible( eye_pos, enemy_eye_pos, pawn, local.pawn ) )
					{
						threat_score -= 15.0f;
					}

					if ( threat_score < best_threat_score )
					{
						best_threat_score = threat_score;
						best_yaw = angle_to_enemy.y - base_yaw_offset;
					}
				}

				return best_threat_score < std::numeric_limits<float>::max( ) ? std::optional<float>{ best_yaw } : std::nullopt;
			};

		if ( settings::g_combat.m_antiaim.at_target.value )
		{
			if ( const auto target_yaw = pick_target_yaw( ) )
			{
				base_yaw = *target_yaw;
			}
		}

		auto indicator = base_yaw;
		if ( this->m_yaw_side == -1 )
		{
			indicator -= 90.0f;
		}
		else if ( this->m_yaw_side == 1 )
		{
			indicator += 90.0f;
		}

		this->m_indicator_yaw = indicator;

		auto yaw = base_yaw;
		if ( this->m_yaw_side == -1 )
		{
			yaw -= 90.0f;
		}
		else if ( this->m_yaw_side == 1 )
		{
			yaw += 90.0f;
		}

		if ( settings::g_combat.m_antiaim.yaw_jitter.value )
		{
			this->m_yaw_jitter_flip = !this->m_yaw_jitter_flip;
			const auto amount = static_cast< float >( settings::g_combat.m_antiaim.yaw_jitter_amount.value );
			yaw += this->m_yaw_jitter_flip ? amount : -amount;
		}

		if ( settings::g_combat.m_antiaim.spin.value )
		{
			this->m_spin += static_cast< float >( settings::g_combat.m_antiaim.spin_speed.value );
			if ( this->m_spin > 360.0f || this->m_spin < -360.0f )
			{
				this->m_spin = std::remainderf( this->m_spin, 360.0f );
			}
			yaw += this->m_spin;
			this->m_indicator_yaw = yaw;
		}
	else if ( std::fabsf( this->m_spin ) > 0.0f )
		{
			this->m_spin = 0.0f;
		}

		if (settings::g_combat.m_antiaim.auto_yaw_adjust.value)
			yaw += 33.0f;

		return yaw;
	}

	void misc::antiaim::correct_movement (systems::input::usercmd* cmd) {
		if (!this->m_should_correct) {
			return;
		}

		this->m_should_correct = false;

		const auto base = cmd->csgo_user_cmd.mutable_base ();
		if (!base) {
			return;
		}

		math::vector3 new_forward {}, new_right {};
		math::helpers::angle_vectors_2d (this->m_modified_angles.y, new_forward, new_right);

		math::vector3 old_forward {}, old_right {};
		math::helpers::angle_vectors_2d (this->m_old_angles.y, old_forward, old_right);

		const auto forward_move = base->forwardmove ();
		const auto side_move = base->leftmove ();

		if (forward_move != 0.0f || side_move != 0.0f) {
			const auto intent = old_forward * forward_move + old_right * side_move;
			const auto intent_len = intent.length ();

			if (intent_len > 0.0f) {
				const auto intent_dir = intent / intent_len;
				const auto corrected_forward = new_forward.dot (intent_dir) * intent_len;
				const auto corrected_side = new_right.dot (intent_dir) * intent_len;

				base->set_forwardmove (std::clamp (corrected_forward, -1.0f, 1.0f));
				base->set_leftmove (std::clamp (corrected_side, -1.0f, 1.0f));
			}
		}

		if (systems::g_prediction.pre ().flags & cstypes::entity_flags::on_ground) {
			auto buttons = cmd->buttons.value;
			buttons &= ~static_cast<std::uintptr_t>(cstypes::command_buttons::in_forward | cstypes::command_buttons::in_back | cstypes::command_buttons::in_moveleft | cstypes::command_buttons::in_moveright);

			if (base->forwardmove () > 0.0f) {
				buttons |= cstypes::command_buttons::in_forward;
			} else if (base->forwardmove () < 0.0f) {
				buttons |= cstypes::command_buttons::in_back;
			}

			if (base->leftmove () > 0.0f) {
				buttons |= cstypes::command_buttons::in_moveleft;
			} else if (base->leftmove () < 0.0f) {
				buttons |= cstypes::command_buttons::in_moveright;
			}

			cmd->buttons.value = buttons;
		}
	}

	bool misc::antiaim::is_near_ladder( std::uintptr_t local_pawn ) const
	{
		return false;
	}

	void misc::antiaim::release_mouse_override_cursor( )
	{
		if ( !this->m_mouse_override_cursor_owned )
		{
			return;
		}

		if ( addresses::globals::input_system && !rendering::g_menu.is_open( ) )
		{
			memory::call_vfunc<void>( addresses::globals::input_system, 76, this->m_mouse_override_saved_relative != 0 );
		}

		this->m_mouse_override_cursor_owned = false;
	}

	void misc::antiaim::update_mouse_override( float view_yaw )
	{
		auto& cfg = settings::g_combat.m_antiaim;
		auto& io = ImGui::GetIO( );
		const bool want = cfg.enabled.value && cfg.mouse_override.value && !rendering::g_menu.is_open( );
		auto reset_to_center{ false };

		if ( want && !this->m_mouse_override_was_held )
		{
			const auto now = GetTickCount64( );
			if ( this->m_mouse_override_last_press_ms && now - this->m_mouse_override_last_press_ms <= 400ull )
			{
				this->m_mouse_override_latched = false;
				this->m_mouse_override_centered = true;
				this->m_mouse_override_x = 0.0f;
				this->m_mouse_override_y = 0.0f;
				this->m_mouse_override_last_press_ms = 0;
				reset_to_center = true;
			}
			else
			{
				this->m_mouse_override_last_press_ms = now;
			}
		}
		this->m_mouse_override_was_held = want;

		if ( !want )
		{
			this->release_mouse_override_cursor( );
			this->m_mouse_override_active = false;
			return;
		}

		if ( !this->m_mouse_override_active )
		{
			this->m_mouse_override_frozen_view = systems::g_input.get_view_angles( );
		}

		this->m_mouse_override_active = true;

		const auto display = io.DisplaySize;
		const auto center_x = display.x * 0.5f;
		const auto center_y = display.y * 0.5f;
		const auto half_h = std::fmaxf( display.y * 0.5f, 1.0f );

		auto view_fov = systems::g_view.fov( );
		if ( !( view_fov > 1.0f && view_fov < 170.0f ) )
		{
			view_fov = 90.0f;
		}

		const auto override_fov = std::clamp( cfg.mouse_override_fov.value, 10.0f, 180.0f );
		constexpr auto k_radius_scale = 0.38f;
		const auto radius = ( std::tanf( ( override_fov * 0.5f ) * ( std::numbers::pi_v<float> / 180.0f ) )
			/ std::tanf( ( view_fov * 0.5f ) * ( std::numbers::pi_v<float> / 180.0f ) ) )
			* half_h * k_radius_scale;
		this->m_mouse_override_radius = radius;

		if ( reset_to_center )
		{
			this->m_mouse_override_x = 0.0f;
			this->m_mouse_override_y = 0.0f;
			this->m_mouse_override_latched = false;
			this->m_mouse_override_centered = true;
		}
		else if ( !this->m_mouse_override_cursor_owned && this->m_mouse_override_latched && !this->m_mouse_override_centered )
		{
			auto offset = this->m_mouse_override_yaw - this->m_mouse_override_frozen_view.y;
			math::helpers::normalize_angle( offset );
			const auto rad = offset * ( std::numbers::pi_v<float> / 180.0f );
			const auto mag = std::fmaxf( radius * 0.72f, 1.0f );
			this->m_mouse_override_x = -std::sinf( rad ) * mag;
			this->m_mouse_override_y = -std::cosf( rad ) * mag;
		}

		if ( !this->m_mouse_override_cursor_owned && addresses::globals::input_system )
		{
			this->m_mouse_override_saved_relative = memory::call_vfunc<std::uint8_t>( addresses::globals::input_system, 77 );
			memory::call_vfunc<void>( addresses::globals::input_system, 76, false );
			this->m_mouse_override_cursor_owned = true;

			const HWND hwnd = rendering::g_context.get_window( );
			if ( hwnd )
			{
				POINT pos{
					static_cast< LONG >( center_x + this->m_mouse_override_x ),
					static_cast< LONG >( center_y + this->m_mouse_override_y )
				};
				ClientToScreen( hwnd, &pos );
				SetCursorPos( pos.x, pos.y );
			}
		}

		if ( addresses::globals::input_system )
		{
			memory::call_vfunc<void>( addresses::globals::input_system, 76, false );
		}

		SetCursor( LoadCursor( nullptr, IDC_ARROW ) );
		io.MouseDrawCursor = false;

		systems::g_input.set_view_angles( this->m_mouse_override_frozen_view );

		if ( reset_to_center )
		{
			( void )view_yaw;
			return;
		}

		float cursor_x = io.MousePos.x;
		float cursor_y = io.MousePos.y;
		const HWND hwnd = rendering::g_context.get_window( );
		if ( hwnd )
		{
			POINT pt{};
			if ( GetCursorPos( &pt ) && ScreenToClient( hwnd, &pt ) )
			{
				cursor_x = static_cast< float >( pt.x );
				cursor_y = static_cast< float >( pt.y );
				io.MousePos.x = cursor_x;
				io.MousePos.y = cursor_y;
			}
		}

		auto dx = cursor_x - center_x;
		auto dy = cursor_y - center_y;
		const auto len = std::sqrtf( dx * dx + dy * dy );
		if ( len > radius && len > 0.0f )
		{
			const auto scale = radius / len;
			dx *= scale;
			dy *= scale;

			if ( hwnd )
			{
				POINT clamped{
					static_cast< LONG >( center_x + dx ),
					static_cast< LONG >( center_y + dy )
				};
				ClientToScreen( hwnd, &clamped );
				SetCursorPos( clamped.x, clamped.y );
			}
		}

		this->m_mouse_override_x = dx;
		this->m_mouse_override_y = dy;

		const auto deadzone = ( std::max )( 14.0f, radius * 0.18f );
		this->m_mouse_override_centered = len < deadzone;
		if ( this->m_mouse_override_centered )
		{
			this->m_mouse_override_latched = false;
			( void )view_yaw;
			return;
		}

		this->m_mouse_override_latched = true;
		const auto yaw_offset = std::atan2f( -dx, -dy ) * ( 180.0f / std::numbers::pi_v<float> );
		this->m_mouse_override_yaw = this->m_mouse_override_frozen_view.y + yaw_offset;
		math::helpers::normalize_angle( this->m_mouse_override_yaw );
		( void )view_yaw;
	}

	void misc::antiaim::draw_mouse_override_indicator( xdraw::draw_list& draw_list ) const
	{
		if ( !this->m_mouse_override_active )
		{
			return;
		}

		const auto& io = ImGui::GetIO( );
		const auto display = io.DisplaySize;
		if ( display.x < 1.0f || display.y < 1.0f )
		{
			return;
		}

		const auto center = math::vector2{ display.x * 0.5f, display.y * 0.5f };
		const auto radius = std::fmaxf( this->m_mouse_override_radius, 24.0f );
		const auto& src = settings::g_combat.m_antiaim.mouse_override_color.value;

		const auto col = [ & ]( float a ) -> xdraw::color
			{
				return { src.r, src.g, src.b, static_cast< std::uint8_t >( std::clamp( a, 0.0f, 1.0f ) * 255.0f ) };
			};

		draw_list.circle( center.x, center.y, radius, col( 0.22f ), 1.35f, 64 );
		draw_list.circle( center.x, center.y, radius * 0.18f, col( 0.12f ), 1.0f, 32 );

		constexpr auto tick_count = 4;
		for ( auto i = 0; i < tick_count; ++i )
		{
			const auto angle = static_cast< float >( i ) * ( std::numbers::pi_v<float> * 0.5f );
			const auto c = std::cosf( angle );
			const auto s = std::sinf( angle );
			const auto inner = radius - ( i == 0 ? 9.0f : 6.0f );
			const auto outer = radius + ( i == 0 ? 3.0f : 2.0f );
			draw_list.line(
				center.x + s * inner, center.y - c * inner,
				center.x + s * outer, center.y - c * outer,
				col( i == 0 ? 0.70f : 0.28f ),
				i == 0 ? 1.7f : 1.1f );
		}

		const auto dot = math::vector2{
			center.x + this->m_mouse_override_x,
			center.y + this->m_mouse_override_y
		};

		if ( !this->m_mouse_override_centered )
		{
			draw_list.line( center.x, center.y, dot.x, dot.y, col( 0.38f ), 1.15f );

			const auto len = std::sqrtf( this->m_mouse_override_x * this->m_mouse_override_x + this->m_mouse_override_y * this->m_mouse_override_y );
			if ( len > 1.0f )
			{
				const auto nx = this->m_mouse_override_x / len;
				const auto ny = this->m_mouse_override_y / len;
				const auto ax = center.x + nx * radius;
				const auto ay = center.y + ny * radius;
				const auto px = -ny;
				const auto py = nx;
				draw_list.triangle_filled(
					ax + nx * 7.0f, ay + ny * 7.0f,
					ax + px * 4.5f - nx * 2.0f, ay + py * 4.5f - ny * 2.0f,
					ax - px * 4.5f - nx * 2.0f, ay - py * 4.5f - ny * 2.0f,
					col( 0.92f ) );
			}
		}

		draw_list.circle_filled( center.x, center.y, 2.2f, col( 0.55f ) );
		draw_list.circle_filled( dot.x, dot.y, 3.4f, col( 0.18f ) );
		draw_list.circle( dot.x, dot.y, 7.0f, col( 0.85f ), 1.45f, 24 );
		draw_list.circle_filled( dot.x, dot.y, 2.6f, col( 1.0f ) );
	}

}



