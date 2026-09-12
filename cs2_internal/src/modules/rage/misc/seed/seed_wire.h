#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>

namespace features::combat::seed_wire {

	namespace detail {

		[[nodiscard]] inline float wrap180( float deg )
		{
			return std::remainderf( deg, 360.0f );
		}

		[[nodiscard]] inline bool hist_protected( int i, int atk_idx, int count )
		{
			if ( count <= 0 ) return true;
			const int tip = count - 1;
			const int lo = ( atk_idx > 0 ) ? ( atk_idx - 1 ) : atk_idx;
			const int hi = ( atk_idx + 1 < count ) ? ( atk_idx + 1 ) : atk_idx;
			const int tip_lo = ( tip > 0 ) ? ( tip - 1 ) : tip;
			if ( i >= lo && i <= hi ) return true;
			if ( i >= tip_lo && i <= tip ) return true;
			return false;
		}

		inline void cloak_base_to_camera( systems::input::usercmd* cmd, const math::vector3& cam )
		{
			if ( !cmd ) return;
			const auto base = cmd->csgo_user_cmd.mutable_base( );
			if ( !base ) return;
			const auto va = base->mutable_viewangles( );
			if ( !va ) return;
			math::vector3 a = cam;
			a.z = 0.0f;
			a.x = std::clamp( a.x, -89.0f, 89.0f );
			math::helpers::normalize_angle( a.y );
			va->set_x( a.x );
			va->set_y( a.y );
			va->set_z( a.z );
		}

		inline void write_plausible_mouse( systems::input::usercmd* cmd, const math::vector3& cam, const math::vector3& fire )
		{
			if ( !cmd ) return;
			const auto base = cmd->csgo_user_cmd.mutable_base( );
			if ( !base ) return;

			const float d_yaw = wrap180( fire.y - cam.y );
			const float d_pitch = fire.x - cam.x;
			const float mag = std::sqrt( d_yaw * d_yaw + d_pitch * d_pitch );
			if ( mag < 0.08f ) return;

			int mx = static_cast<int>( std::lround( d_yaw * 18.0f ) );
			int my = static_cast<int>( std::lround( -d_pitch * 18.0f ) );
			mx = std::clamp( mx, -512, 512 );
			my = std::clamp( my, -512, 512 );
			if ( mx == 0 && my == 0 ) {
				mx = ( d_yaw >= 0.0f ) ? 1 : -1;
				my = ( d_pitch <= 0.0f ) ? 1 : -1;
			}

			const int cur_x = base->mousedx( );
			const int cur_y = base->mousedy( );
			if ( std::abs( cur_x ) > std::abs( mx ) ) mx = cur_x;
			if ( std::abs( cur_y ) > std::abs( my ) ) my = cur_y;
			base->set_mousedx( mx );
			base->set_mousedy( my );
		}

		inline void soften_unprotected( systems::input::usercmd* cmd, int atk_idx, const math::vector3& cam, const math::vector3& fire )
		{
			if ( !cmd ) return;
			const int count = cmd->csgo_user_cmd.input_history_size( );
			if ( count <= 1 ) return;
			if ( atk_idx < 0 || atk_idx >= count ) atk_idx = count - 1;

			const float d_yaw = wrap180( fire.y - cam.y );
			const float d_pitch = fire.x - cam.x;
			if ( std::fabs( d_yaw ) < 0.05f && std::fabs( d_pitch ) < 0.05f ) return;

			for ( int i = 0; i < count; ++i ) {
				if ( hist_protected( i, atk_idx, count ) ) continue;
				auto* e = cmd->csgo_user_cmd.mutable_input_history( i );
				if ( !e ) continue;
				auto* va = e->mutable_view_angles( );
				if ( !va ) continue;

				const float t = static_cast<float>( i + 1 ) / static_cast<float>( atk_idx + 1 );
				const float clamped = std::clamp( t, 0.0f, 1.0f );
				const float s = clamped * clamped * ( 3.0f - 2.0f * clamped );
				math::vector3 a{};
				a.x = cam.x + ( fire.x - cam.x ) * s;
				float dy = wrap180( fire.y - cam.y );
				a.y = cam.y + dy * s;
				a.z = 0.0f;
				a.x = std::clamp( a.x, -89.0f, 89.0f );
				math::helpers::normalize_angle( a.y );
				va->set_x( a.x );
				va->set_y( a.y );
				va->set_z( a.z );
			}
		}

		inline void sync_primary_player_tick( systems::input::usercmd* cmd, int atk_idx, int seed_tick, float seed_frac )
		{
			if ( !cmd || seed_tick <= 0 ) return;
			auto* e = cmd->csgo_user_cmd.mutable_input_history( atk_idx );
			if ( !e ) return;
			float frac = seed_frac;
			if ( !std::isfinite( frac ) || frac < 0.0f || frac >= 1.0f ) frac = 0.0f;
			e->set_player_tick_count( seed_tick );
			e->set_player_tick_fraction( frac );
			if ( e->render_tick_count( ) <= 0 ) {
				e->set_render_tick_count( seed_tick );
				e->set_render_tick_fraction( frac );
			}
		}

	}

	[[nodiscard]] inline int prefer_tip_hist_index( systems::input::usercmd* cmd, int current_idx, const math::vector3* cam_ang )
	{
		if ( !cmd ) return current_idx;
		const int count = cmd->csgo_user_cmd.input_history_size( );
		if ( count <= 0 ) return current_idx;

		const int tip = count - 1;
		auto* te = cmd->csgo_user_cmd.mutable_input_history( tip );

		if ( te && te->player_tick_count( ) > 0 ) {
			if ( cam_ang && te->view_angles( ) ) {
				math::vector3 ha{ te->view_angles( )->x( ), te->view_angles( )->y( ), 0.0f };
				math::vector3 ca = *cam_ang; ca.z = 0.0f;
				float dy = detail::wrap180( ha.y - ca.y );
				const float d = std::sqrt( ( ha.x - ca.x ) * ( ha.x - ca.x ) + dy * dy );
				if ( d > 4.0f && current_idx >= 0 && current_idx < count ) {
					auto* cur = cmd->csgo_user_cmd.mutable_input_history( current_idx );
					if ( cur && cur->player_tick_count( ) > 0 )
						return current_idx;
				}
			}
			return tip;
		}

		if ( current_idx >= 0 && current_idx < count ) {
			auto* cur = cmd->csgo_user_cmd.mutable_input_history( current_idx );
			if ( cur && cur->player_tick_count( ) > 0 )
				return current_idx;
		}
		return tip;
	}

	// Noobchair SetAttackHistoryFire — stamp solved fire angles + eye on attack
	// neighbors so FillGunFireData / SPREADSEEDGEN see the same inputs Solve used.
	inline void set_attack_history_fire( systems::input::usercmd* cmd, int hist_index,
		const math::vector3& fire_ang, const math::vector3& shoot_pos )
	{
		if ( !cmd ) return;
		const int count = cmd->csgo_user_cmd.input_history_size( );
		if ( count <= 0 ) return;

		int idx = hist_index;
		if ( idx < 0 || idx >= count )
			idx = count - 1;

		cmd->csgo_user_cmd.set_attack1_start_history_index( idx );

		const int lo = ( idx > 0 ) ? ( idx - 1 ) : idx;
		const int hi = ( idx + 1 < count ) ? ( idx + 1 ) : idx;
		const int tip = count - 1;
		const int tip_lo = ( tip > 0 ) ? ( tip - 1 ) : tip;
		const int lo2 = ( lo < tip_lo ) ? lo : tip_lo;
		const int hi2 = ( hi > tip ) ? hi : tip;

		auto* primary = cmd->csgo_user_cmd.mutable_input_history( idx );
		const int keep_tick = ( primary && primary->player_tick_count( ) > 0 )
			? primary->player_tick_count( ) : 0;
		const float keep_frac = ( primary && std::isfinite( primary->player_tick_fraction( ) ) )
			? primary->player_tick_fraction( ) : 0.0f;

		math::vector3 ang = fire_ang;
		if ( !std::isfinite( ang.z ) ) ang.z = 0.0f;
		ang.x = std::clamp( ang.x, -89.0f, 89.0f );
		math::helpers::normalize_angle( ang.y );

		for ( int i = lo2; i <= hi2; ++i ) {
			auto* e = cmd->csgo_user_cmd.mutable_input_history( i );
			if ( !e ) continue;

			if ( auto* va = e->mutable_view_angles( ) ) {
				va->set_x( ang.x );
				va->set_y( ang.y );
				va->set_z( ang.z );
			}
			// Never invent shoot_position — null nested msg + has-bit = client AV [rcx+0x10].
			if ( e->has_shoot_position( ) ) {
				if ( auto* sp = e->mutable_shoot_position( ) ) {
					sp->set_x( shoot_pos.x );
					sp->set_y( shoot_pos.y );
					sp->set_z( shoot_pos.z );
				}
			}
			if ( keep_tick > 0 ) {
				e->set_player_tick_count( keep_tick );
				e->set_player_tick_fraction( keep_frac );
				const int rt = e->render_tick_count( );
				if ( rt <= 0 || std::abs( rt - keep_tick ) > 1 ) {
					e->set_render_tick_count( keep_tick );
					e->set_render_tick_fraction( keep_frac );
				}
			}
		}
	}

	inline void finalize_seed_trigger_fire( systems::input::usercmd* cmd, int atk_idx,
		const math::vector3& fire_ang, const math::vector3& /*cam_ang*/,
		const math::vector3& seed_eye, int seed_tick, float seed_frac )
	{
		if ( !cmd || seed_tick <= 0 ) return;
		const int count = cmd->csgo_user_cmd.input_history_size( );
		if ( atk_idx < 0 || atk_idx >= count )
			atk_idx = count > 0 ? count - 1 : -1;
		if ( atk_idx < 0 ) return;

		// Player tick MUST be on primary BEFORE neighbor stamp (interp copies it).
		detail::sync_primary_player_tick( cmd, atk_idx, seed_tick, seed_frac );
		set_attack_history_fire( cmd, atk_idx, fire_ang, seed_eye );
	}

}
