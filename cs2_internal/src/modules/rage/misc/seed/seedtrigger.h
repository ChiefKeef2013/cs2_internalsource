#pragma once

#include <modules/rage/misc/seed/hitchance.h>
#include <modules/rage/misc/seed/nospread_seed.h>
#include <modules/rage/misc/seed/seed_wire.h>

namespace features::combat::seed_trigger {

	[[nodiscard]] inline bool init( )
	{
		return hitchance::init( );
	}

	[[nodiscard]] inline bool ready( )
	{
		return hitchance::spread_seed_ready( );
	}

	using shot = seed_nospread::shot;

	[[nodiscard]] inline bool safe_fill_gun_fire_data( std::uintptr_t weapon, hitchance::gun_fire_data& out )
	{
		out = {};
		if ( !weapon ) return false;
		return hitchance::fill_gun_fire_data( weapon, out );
	}

	[[nodiscard]] inline bool safe_exact_shot_hits_any( const math::vector3& eye, const math::vector3& fire_angles,
		int seed_tick, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		const bool* enabled_hbs, int* out_hb, math::vector3* out_pt, float tick_frac )
	{
		if ( !weapon || !local_pawn || !target ) return false;
		return hitchance::exact_shot_hits_any( eye, fire_angles, seed_tick, weapon, local_pawn, target, enabled_hbs, out_hb, out_pt, tick_frac );
	}

	[[nodiscard]] inline bool safe_solve( const math::vector3& eye, const math::vector3& wish_view,
		int seed_tick, float tick_frac, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		int prefer_hb, const bool* enabled_hbs, shot& out )
	{
		if ( !weapon || !local_pawn || !target ) return false;
		return seed_nospread::solve( eye, wish_view, seed_tick, tick_frac, weapon, local_pawn, target, prefer_hb, enabled_hbs, out );
	}

	[[nodiscard]] inline bool seed_cycle_allows_fire( std::uintptr_t weapon, std::uintptr_t local_pawn )
	{
		return seed_nospread::seed_cycle_allows_fire( weapon, local_pawn );
	}

	inline void note_seed_fired( std::uintptr_t weapon, std::uintptr_t local_pawn = 0 )
	{
		seed_nospread::note_seed_fired( weapon, local_pawn );
	}

	[[nodiscard]] inline bool seed_passes_damage( const math::vector3& eye, math::vector3& inout_point,
		int& inout_hb_idx, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		bool allow_pen, float min_damage )
	{
		return seed_nospread::seed_passes_damage( eye, inout_point, inout_hb_idx,
			weapon, local_pawn, target, allow_pen, min_damage );
	}

	inline void finalize_seed_trigger_fire( systems::input::usercmd* cmd, int atk_idx,
		const math::vector3& fire_ang, const math::vector3& cam_ang,
		const math::vector3& seed_eye, int seed_tick, float seed_frac )
	{
		seed_wire::finalize_seed_trigger_fire( cmd, atk_idx, fire_ang, cam_ang, seed_eye, seed_tick, seed_frac );
	}

	[[nodiscard]] inline int prefer_tip_hist_index( systems::input::usercmd* cmd, int current_idx, const math::vector3* cam_ang )
	{
		return seed_wire::prefer_tip_hist_index( cmd, current_idx, cam_ang );
	}

}
