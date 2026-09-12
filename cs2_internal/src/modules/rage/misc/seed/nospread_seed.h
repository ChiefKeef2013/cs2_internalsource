#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <modules/rage/misc/seed/hitchance.h>

namespace features::combat::seed_nospread {

	using hitchance::hb_head;
	using hitchance::hb_neck;
	using hitchance::hb_chest;
	using hitchance::hb_stomach;
	using hitchance::hb_pelvis;
	using hitchance::hb_arms;
	using hitchance::hb_legs;
	using hitchance::hb_feet;
	using hitchance::hb_count;

	struct shot
	{
		math::vector3 fire_angles{};
		math::vector3 hit_point{};
		int hitbox_idx{ -1 };
		int hb_category{ -1 };
		int seed_tick{};
		float seed_frac{};
		float sx{};
		float sy{};
		bool ok{};
	};

	namespace detail {

		constexpr float k_pi = 3.14159265358979323846f;
		constexpr float k_rad_to_deg = 180.0f / k_pi;

		[[nodiscard]] inline bool is_heavy_pistol( std::uintptr_t weapon )
		{
			const auto def = hitchance::detail::weapon_def_idx( weapon );
			return def == cstypes::item_definition_index::weapon_desert_eagle
				|| def == cstypes::item_definition_index::weapon_r8_revolver;
		}

		[[nodiscard]] inline bool is_sniper( std::uintptr_t weapon )
		{
			const auto def = hitchance::detail::weapon_def_idx( weapon );
			return def == cstypes::item_definition_index::weapon_awp
				|| def == cstypes::item_definition_index::weapon_ssg_08
				|| def == cstypes::item_definition_index::weapon_scar_20
				|| def == cstypes::item_definition_index::weapon_g3sg1;
		}

		[[nodiscard]] inline bool is_local_scoped( std::uintptr_t local_pawn )
		{
			return local_pawn && systems::g_entities.is_cs_player_pawn( local_pawn )
				&& reinterpret_cast<C_CSPlayerPawn*>( local_pawn )->m_bIsScoped( );
		}

		[[nodiscard]] inline bool local_on_ground( std::uintptr_t local_pawn )
		{
			if ( !local_pawn ) return true;
			const auto flags = reinterpret_cast<C_BaseEntity*>( local_pawn )->m_fFlags( );
			return ( flags & cstypes::entity_flags::on_ground ) != 0;
		}

		[[nodiscard]] inline bool dir_to_angles( const math::vector3& dir, math::vector3& out )
		{
			const float hyp = std::sqrt( dir.x * dir.x + dir.y * dir.y );
			if ( !std::isfinite( hyp ) || hyp < 1e-8f ) return false;
			out.x = -std::atan2( dir.z, hyp ) * k_rad_to_deg;
			out.y = std::atan2( dir.y, dir.x ) * k_rad_to_deg;
			out.z = 0.0f;
			math::helpers::normalize_angle( out.y );
			out.x = std::clamp( out.x, -89.0f, 89.0f );
			return hitchance::detail::is_valid_ang( out );
		}

		[[nodiscard]] inline float ang_delta( const math::vector3& a, const math::vector3& b )
		{
			const float dp = a.x - b.x;
			float dy = a.y - b.y;
			math::helpers::normalize_angle( dy );
			return std::sqrt( dp * dp + dy * dy );
		}

		[[nodiscard]] inline bool normalize_dir( math::vector3& d )
		{
			const float ls = d.length_sqr( );
			if ( !std::isfinite( ls ) || ls < 1e-12f ) return false;
			d *= ( 1.0f / std::sqrt( ls ) );
			return true;
		}

		[[nodiscard]] inline float quant_half( float a )
		{
			math::helpers::normalize_angle( a );
			return std::floor( a * 2.0f ) * 0.5f;
		}

		// Iterative inverse of FireBullet basis: given the desired forward dir + spread (sx,sy),
		// find the punched view whose basis makes the pellet land on wantDir.
		[[nodiscard]] inline bool solve_view_for_dir( const math::vector3& want_dir, float sx, float sy,
			const math::vector3& punched_start, math::vector3& out_punched )
		{
			math::vector3 punched = punched_start;
			punched.z = 0.0f;
			math::helpers::normalize_angle( punched.y );
			if ( !hitchance::detail::is_valid_ang( punched ) ) return false;
			for ( int it = 0; it < 12; ++it ) {
				math::vector3 fwd{}, right{}, up{};
				punched.to_directions( &fwd, &right, &up );
				math::vector3 ideal{
					want_dir.x + right.x * sx - up.x * sy,
					want_dir.y + right.y * sx - up.y * sy,
					want_dir.z + right.z * sx - up.z * sy };
				if ( !normalize_dir( ideal ) ) return false;
				math::vector3 next{};
				if ( !dir_to_angles( ideal, next ) ) return false;
				if ( ang_delta( punched, next ) < 0.008f ) {
					out_punched = next;
					return true;
				}
				punched = next;
			}
			out_punched = punched;
			return hitchance::detail::is_valid_ang( out_punched );
		}

		// Roll-trick: keep same seed bin (pitch/yaw quant unchanged) by rotating basis.
		[[nodiscard]] inline bool roll_trick_view( const math::vector3& wish_unpunched,
			const math::vector3* punch, int seed_tick, float sx, float sy, math::vector3& out_view )
		{
			if ( seed_tick <= 0 ) return false;
			if ( !std::isfinite( sx ) || !std::isfinite( sy ) ) return false;
			math::vector3 punched_wish = wish_unpunched;
			if ( punch && hitchance::detail::is_valid_ang( *punch ) ) {
				punched_wish.x += punch->x;
				punched_wish.y += punch->y;
			}
			punched_wish.z = 0.0f;
			math::helpers::normalize_angle( punched_wish.y );

			const float len = std::sqrt( sx * sx + sy * sy );
			if ( !std::isfinite( len ) || len < 1e-8f ) return false;

			const std::uint32_t seed0 = hitchance::compute_seed( punched_wish, seed_tick );
			const float pitch_adj = std::atan( len ) * k_rad_to_deg;
			const float roll = -std::atan2( sx, sy ) * k_rad_to_deg;
			const float base_pitch = punched_wish.x + pitch_adj;

			float last_qx = 1e9f;
			for ( int k = 0; k < 11; ++k ) {
				float try_pitch = base_pitch;
				if ( k > 0 ) {
					const int step = ( k + 1 ) / 2;
					const float sign = ( k % 2 == 1 ) ? 1.0f : -1.0f;
					try_pitch += sign * static_cast<float>( step ) * 0.5f;
				}
				try_pitch = std::clamp( try_pitch, -89.0f, 89.0f );
				const float qx = quant_half( try_pitch );
				if ( k > 0 && std::fabs( qx - last_qx ) < 1e-4f ) continue;
				last_qx = qx;

				math::vector3 punched_cand{ try_pitch, punched_wish.y, roll };
				math::helpers::normalize_angle( punched_cand.y );
				if ( hitchance::compute_seed( punched_cand, seed_tick ) != seed0 ) continue;

				math::vector3 view = punched_cand;
				if ( punch && hitchance::detail::is_valid_ang( *punch ) ) {
					view.x -= punch->x;
					view.y -= punch->y;
				}
				view.x = std::clamp( view.x, -89.0f, 89.0f );
				math::helpers::normalize_angle( view.y );
				if ( !hitchance::detail::is_valid_ang( view ) ) continue;
				out_view = view;
				return true;
			}
			return false;
		}

		[[nodiscard]] inline float max_delta_deg( float inac, float spr, std::uintptr_t weapon, std::uintptr_t local_pawn )
		{
			const float bloom = ( std::isfinite( inac ) && std::isfinite( spr ) ) ? ( inac + spr ) : 0.0f;
			const float cone = bloom * k_rad_to_deg;
			const bool air = !local_on_ground( local_pawn );
			const bool heavy = is_heavy_pistol( weapon );
			const bool sniper = is_sniper( weapon );

			if ( heavy ) {
				const float d = std::max( air ? 16.0f : 8.0f, cone * ( air ? 2.6f : 1.5f ) + ( air ? 6.0f : 2.0f ) );
				return std::clamp( d, air ? 16.0f : 8.0f, air ? 72.0f : 32.0f );
			}
			if ( sniper ) {
				const bool scoped = is_local_scoped( local_pawn );
				if ( !scoped && air ) {
					const float d = std::max( 5.0f, cone * 0.9f + 1.0f );
					return std::clamp( d, 5.0f, 8.0f );
				}
				if ( !scoped ) {
					const float d = std::max( 5.0f, cone * 1.0f + 1.5f );
					return std::clamp( d, 5.0f, 12.0f );
				}
				const float d = std::max( air ? 6.0f : 5.0f, cone * 1.15f + 1.5f );
				return std::clamp( d, air ? 6.0f : 5.0f, air ? 14.0f : 18.0f );
			}
			const float d = std::max( 10.0f, cone * 2.0f + 3.0f );
			return std::clamp( d, 10.0f, air ? 48.0f : 56.0f );
		}

		[[nodiscard]] inline bool is_core( int hb )
		{
			return hb == hb_head || hb == hb_neck || hb == hb_chest || hb == hb_stomach || hb == hb_pelvis;
		}

		// Build a search-time pellet dir (local ran1).
		[[nodiscard]] inline bool bullet_dir_local( const math::vector3& view, int seed_tick, std::uintptr_t weapon,
			float inac, float spr, const math::vector3* punch, math::vector3& out_dir, float* out_sx, float* out_sy )
		{
			return hitchance::get_bullet_direction_cached( view, seed_tick, weapon, inac, spr,
				out_dir, out_sx, out_sy, 1u, punch, true );
		}

		[[nodiscard]] inline bool bullet_dir_game( const math::vector3& view, int seed_tick, std::uintptr_t weapon,
			float inac, float spr, const math::vector3* punch, math::vector3& out_dir, float* out_sx, float* out_sy )
		{
			return hitchance::get_bullet_direction_cached( view, seed_tick, weapon, inac, spr,
				out_dir, out_sx, out_sy, 1u, punch, false );
		}

		// Iterate all enabled hitbox categories and return the first that the ray hits.
		[[nodiscard]] inline bool ray_hits_any( std::uintptr_t target, const math::vector3& eye,
			const math::vector3& dir, int prefer_hb, const bool* enabled,
			int& out_hb_cat, int& out_hitbox_idx, math::vector3& out_pt, float scale )
		{
			static constexpr int k_core[] = { hb_head, hb_neck, hb_chest, hb_stomach, hb_pelvis };
			static constexpr int k_limb[] = { hb_arms, hb_legs, hb_feet };
			int list[ 12 ]{};
			int n = 0;
			auto push = [ & ] ( int hb ) {
				if ( hb < 0 || hb >= hb_count ) return;
				if ( enabled && !enabled[ hb ] ) return;
				for ( int i = 0; i < n; ++i ) if ( list[ i ] == hb ) return;
				list[ n++ ] = hb;
				};
			if ( prefer_hb >= 0 ) push( prefer_hb );
			for ( int hb : k_core ) push( hb );
			for ( int hb : k_limb ) push( hb );
			for ( int i = 0; i < n; ++i ) {
				const int idx = hitchance::detail::pick_hitbox_for_category( target, list[ i ] );
				if ( idx < 0 ) continue;
				if ( !hitchance::detail::ray_hits_hitbox( target, idx, eye, dir, scale, &out_pt ) )
					continue;
				out_hb_cat = list[ i ];
				out_hitbox_idx = idx;
				return true;
			}
			return false;
		}

		struct try_ctx
		{
			bool local_air{};
			bool heavy{};
			bool sniper{};
			float accept_scale{ 0.96f };
			float game_scale{ 0.98f };
			bool core_prefer_gate{};
		};

		inline void build_try_ctx( std::uintptr_t weapon, std::uintptr_t local_pawn, float inac, float spr, try_ctx& c )
		{
			c = {};
			c.heavy = is_heavy_pistol( weapon );
			c.sniper = is_sniper( weapon );
			c.local_air = !local_on_ground( local_pawn );

			if ( c.heavy && c.local_air ) {
				c.accept_scale = 1.05f;
				c.game_scale = 1.05f;
			}
			else {
				const float bloom = ( std::isfinite( inac ) && std::isfinite( spr ) ) ? ( inac + spr ) : 0.0f;
				if ( bloom > 0.25f || c.sniper ) c.game_scale = 0.96f;
				c.core_prefer_gate = !c.local_air && !c.heavy && bloom > 0.28f;
			}
		}

		[[nodiscard]] inline bool try_view( const math::vector3& eye, const math::vector3& view, int seed_tick, float tick_frac,
			std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
			float inac, float spr, const math::vector3* punch, int prefer_hb, const bool* enabled,
			float max_delta, const math::vector3& wish, const try_ctx& ctx, shot& best, bool verify_game = true )
		{
			math::vector3 wish_py = wish; wish_py.z = 0.0f;
			math::vector3 view_py = view; view_py.z = 0.0f;
			if ( !hitchance::detail::is_valid_ang( view ) || ang_delta( wish_py, view_py ) > max_delta + 0.05f )
				return false;

			math::vector3 cand = view;
			if ( !std::isfinite( cand.z ) ) cand.z = 0.0f;
			math::helpers::normalize_angle( cand.y );
			cand.x = std::clamp( cand.x, -89.0f, 89.0f );

			math::vector3 dir{};
			float sx = 0.0f, sy = 0.0f;
			if ( !bullet_dir_local( cand, seed_tick, weapon, inac, spr, punch, dir, &sx, &sy ) )
				return false;

			int hb_cat = -1, hitbox_idx = -1;
			math::vector3 pt{};
			if ( !ray_hits_any( target, eye, dir, prefer_hb, enabled, hb_cat, hitbox_idx, pt, ctx.accept_scale ) )
				return false;

			if ( verify_game ) {
				math::vector3 g_dir{};
				float gsx = 0.0f, gsy = 0.0f;
				if ( bullet_dir_game( cand, seed_tick, weapon, inac, spr, punch, g_dir, &gsx, &gsy ) ) {
					int g_cat = -1, g_idx = -1;
					math::vector3 g_pt{};
					if ( !ray_hits_any( target, eye, g_dir, prefer_hb, enabled, g_cat, g_idx, g_pt, ctx.game_scale ) )
						return false;
					hb_cat = g_cat;
					hitbox_idx = g_idx;
					pt = g_pt;
					sx = gsx; sy = gsy;
					dir = g_dir;
				}
			}

			if ( ctx.core_prefer_gate && !is_core( hb_cat ) && prefer_hb >= 0 && is_core( prefer_hb ) ) {
				bool any_core = false;
				if ( enabled ) {
					for ( int i = 0; i < hb_count; ++i ) if ( enabled[ i ] && is_core( i ) ) { any_core = true; break; }
				}
				else any_core = true;
				if ( any_core ) return false;
			}

			best.fire_angles = cand;
			best.hit_point = pt;
			best.hitbox_idx = hitbox_idx;
			best.hb_category = hb_cat;
			best.seed_tick = seed_tick;
			best.seed_frac = tick_frac;
			best.sx = sx;
			best.sy = sy;
			best.ok = true;
			return true;
		}

		[[nodiscard]] inline bool accept_view( const math::vector3& eye, const math::vector3& view, int seed_tick, float tick_frac,
			std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
			float inac, float spr, const math::vector3* punch, int prefer_hb, const bool* enabled,
			float max_delta, const math::vector3& wish, const try_ctx& ctx, shot& best )
		{
			shot tmp{};
			if ( !try_view( eye, view, seed_tick, tick_frac, weapon, local_pawn, target, inac, spr, punch,
				prefer_hb, enabled, max_delta, wish, ctx, tmp, false ) )
				return false;

			math::vector3 g_dir{};
			float gsx = 0.0f, gsy = 0.0f;
			if ( bullet_dir_game( tmp.fire_angles, seed_tick, weapon, inac, spr, punch, g_dir, &gsx, &gsy ) ) {
				int g_cat = -1, g_idx = -1;
				math::vector3 g_pt{};
				if ( !ray_hits_any( target, eye, g_dir, prefer_hb, enabled, g_cat, g_idx, g_pt, ctx.game_scale ) )
					return false;
				tmp.hb_category = g_cat;
				tmp.hitbox_idx = g_idx;
				tmp.hit_point = g_pt;
				tmp.sx = gsx;
				tmp.sy = gsy;
			}
			if ( ctx.core_prefer_gate && !is_core( tmp.hb_category ) && prefer_hb >= 0 && is_core( prefer_hb ) ) {
				bool any_core = false;
				if ( enabled ) {
					for ( int i = 0; i < hb_count; ++i ) if ( enabled[ i ] && is_core( i ) ) { any_core = true; break; }
				}
				else any_core = true;
				if ( any_core ) return false;
			}
			best = tmp;
			return best.ok;
		}

	}

	[[nodiscard]] inline bool ready( )
	{
		return hitchance::spread_seed_ready( );
	}

	[[nodiscard]] inline bool init( )
	{
		return hitchance::init( );
	}

	// SolveForAim — iterate strategies (natural → roll-trick → closed-form invert → half-deg bins → cone spiral → geometric).
	[[nodiscard]] inline bool solve_for_aim( const math::vector3& eye, const math::vector3& wish_in,
		const math::vector3& aim_pt, int prefer_hb, int seed_tick, float tick_frac,
		std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		float inac, float spr, const math::vector3* punch_ptr, const bool* enabled_hbs,
		const detail::try_ctx& ctx, shot& out, float max_delta_floor )
	{
		if ( !hitchance::detail::is_valid_vec( aim_pt ) ) return false;

		math::vector3 wish = wish_in;
		wish.z = 0.0f;
		math::helpers::normalize_angle( wish.y );

		// Geometric wish toward aim point with punch subtracted
		{
			auto to_aim = math::helpers::calculate_angle( eye, aim_pt );
			if ( punch_ptr && hitchance::detail::is_valid_ang( *punch_ptr ) ) {
				to_aim.x -= punch_ptr->x;
				to_aim.y -= punch_ptr->y;
			}
			to_aim.z = 0.0f;
			to_aim.x = std::clamp( to_aim.x, -89.0f, 89.0f );
			math::helpers::normalize_angle( to_aim.y );
			if ( hitchance::detail::is_valid_ang( to_aim ) )
				wish = to_aim;
		}

		float max_delta = detail::max_delta_deg( inac, spr, weapon, local_pawn );
		if ( max_delta_floor > 0.0f )
			max_delta = std::max( max_delta, max_delta_floor );

		math::vector3 want_dir{};
		{
			auto to_aim = math::helpers::calculate_angle( eye, aim_pt );
			to_aim.to_directions( &want_dir, nullptr, nullptr );
			if ( !detail::normalize_dir( want_dir ) ) return false;
		}

		out.seed_frac = tick_frac;

		// Shared wish pellet (one BulletDir) — reused by roll + closed-form.
		math::vector3 wish_dir{};
		float wish_sx = 0.0f, wish_sy = 0.0f;
		const bool have_wish_pellet = detail::bullet_dir_local( wish, seed_tick, weapon, inac, spr, punch_ptr,
			wish_dir, &wish_sx, &wish_sy ) && std::isfinite( wish_sx ) && std::isfinite( wish_sy );

		// 0) Natural seed first — zero rewrite when crosshair already good.
		if ( detail::accept_view( eye, wish, seed_tick, tick_frac, weapon, local_pawn, target, inac, spr,
			punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
			return true;

		// 1) Roll-trick — reuses wishSx/Sy (no second BulletDir)
		if ( have_wish_pellet ) {
			math::vector3 roll_view{};
			if ( detail::roll_trick_view( wish, punch_ptr, seed_tick, wish_sx, wish_sy, roll_view ) ) {
				if ( detail::accept_view( eye, roll_view, seed_tick, tick_frac, weapon, local_pawn, target,
					inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
					return true;
			}
		}

		// 2) Closed-form invert + roll (reuse wish pellet; 1 refine iter max)
		if ( have_wish_pellet ) {
			math::vector3 punched_wish = wish;
			if ( punch_ptr ) { punched_wish.x += punch_ptr->x; punched_wish.y += punch_ptr->y; }
			punched_wish.z = 0.0f;
			math::helpers::normalize_angle( punched_wish.y );

			math::vector3 punched_solve{};
			if ( detail::solve_view_for_dir( want_dir, wish_sx, wish_sy, punched_wish, punched_solve ) ) {
				math::vector3 view_solve = punched_solve;
				if ( punch_ptr ) { view_solve.x -= punch_ptr->x; view_solve.y -= punch_ptr->y; }
				view_solve.z = 0.0f;
				view_solve.x = std::clamp( view_solve.x, -89.0f, 89.0f );
				math::helpers::normalize_angle( view_solve.y );

				math::vector3 rv{};
				if ( detail::roll_trick_view( view_solve, punch_ptr, seed_tick, wish_sx, wish_sy, rv ) ) {
					if ( detail::accept_view( eye, rv, seed_tick, tick_frac, weapon, local_pawn, target,
						inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
						return true;
				}
				if ( detail::accept_view( eye, view_solve, seed_tick, tick_frac, weapon, local_pawn, target,
					inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
					return true;

				// One refine pass
				math::vector3 rdir{};
				float nsx = 0.0f, nsy = 0.0f;
				if ( detail::bullet_dir_local( view_solve, seed_tick, weapon, inac, spr, punch_ptr, rdir, &nsx, &nsy ) ) {
					math::vector3 pd = view_solve;
					if ( punch_ptr ) { pd.x += punch_ptr->x; pd.y += punch_ptr->y; }
					pd.z = 0.0f;
					math::helpers::normalize_angle( pd.y );
					math::vector3 next_p{};
					if ( detail::solve_view_for_dir( want_dir, nsx, nsy, pd, next_p ) ) {
						math::vector3 next = next_p;
						if ( punch_ptr ) { next.x -= punch_ptr->x; next.y -= punch_ptr->y; }
						next.z = 0.0f;
						next.x = std::clamp( next.x, -89.0f, 89.0f );
						math::helpers::normalize_angle( next.y );

						math::vector3 wpy = wish; wpy.z = 0.0f;
						math::vector3 npy = next; npy.z = 0.0f;
						if ( hitchance::detail::is_valid_ang( next ) && detail::ang_delta( wpy, npy ) <= max_delta ) {
							math::vector3 rv2{};
							if ( detail::roll_trick_view( next, punch_ptr, seed_tick, nsx, nsy, rv2 ) ) {
								if ( detail::accept_view( eye, rv2, seed_tick, tick_frac, weapon, local_pawn, target,
									inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
									return true;
							}
							if ( detail::accept_view( eye, next, seed_tick, tick_frac, weapon, local_pawn, target,
								inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
								return true;
						}
					}
				}
			}
		}

		// Unscoped sniper air: skip wide search.
		const bool skip_wide = ctx.sniper && ctx.local_air && !detail::is_local_scoped( local_pawn );

		// 2.5) Tight 0.25° ring — refines near-wish candidates before the wider half-deg sweep.
		if ( !skip_wide ) {
			static constexpr float k_fine_r[] = { 0.25f, 0.5f };
			static constexpr int k_fine_steps = 8;
			constexpr float k_two_pi = 6.28318530717958647692f;
			for ( float r : k_fine_r ) {
				if ( r > max_delta ) break;
				for ( int i = 0; i < k_fine_steps; ++i ) {
					const float th = ( static_cast<float>( i ) / static_cast<float>( k_fine_steps ) ) * k_two_pi;
					math::vector3 c = wish;
					c.x += r * std::cos( th );
					c.y += r * std::sin( th );
					c.z = 0.0f;
					c.x = std::clamp( c.x, -89.0f, 89.0f );
					math::helpers::normalize_angle( c.y );
					math::vector3 wpy = wish; wpy.z = 0.0f;
					if ( detail::ang_delta( wpy, c ) > max_delta ) continue;

					if ( detail::accept_view( eye, c, seed_tick, tick_frac, weapon, local_pawn, target, inac, spr,
						punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
						return true;

					math::vector3 fd{};
					float fsx = 0.0f, fsy = 0.0f;
					if ( detail::bullet_dir_local( c, seed_tick, weapon, inac, spr, punch_ptr, fd, &fsx, &fsy ) ) {
						math::vector3 rv{};
						if ( detail::roll_trick_view( c, punch_ptr, seed_tick, fsx, fsy, rv ) ) {
							if ( detail::accept_view( eye, rv, seed_tick, tick_frac, weapon, local_pawn, target,
								inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
								return true;
						}
					}
				}
			}
		}

		// 3) Half-deg bins — 4x4
		if ( !skip_wide ) {
			static constexpr float k_step[] = { -1.5f, -0.5f, 0.5f, 1.5f };
			for ( float dx : k_step ) {
				for ( float dy : k_step ) {
					math::vector3 c = wish;
					c.x += dx; c.y += dy; c.z = 0.0f;
					c.x = std::clamp( c.x, -89.0f, 89.0f );
					math::helpers::normalize_angle( c.y );
					math::vector3 wpy = wish; wpy.z = 0.0f;
					if ( detail::ang_delta( wpy, c ) > max_delta ) continue;

					shot probe{};
					if ( detail::try_view( eye, c, seed_tick, tick_frac, weapon, local_pawn, target, inac, spr,
						punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, probe, false ) ) {
						if ( detail::accept_view( eye, c, seed_tick, tick_frac, weapon, local_pawn, target,
							inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
							return true;
						continue;
					}
					math::vector3 d{};
					float sx = 0.0f, sy = 0.0f;
					if ( detail::bullet_dir_local( c, seed_tick, weapon, inac, spr, punch_ptr, d, &sx, &sy ) ) {
						math::vector3 rv{};
						if ( detail::roll_trick_view( c, punch_ptr, seed_tick, sx, sy, rv ) ) {
							if ( detail::accept_view( eye, rv, seed_tick, tick_frac, weapon, local_pawn, target,
								inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
								return true;
						}
					}
				}
			}
		}

		// 4) Cone spiral (golden angle)
		if ( !skip_wide ) {
			const float bloom = ( std::isfinite( inac ) && std::isfinite( spr ) ) ? ( inac + spr ) : 0.0f;
			int budget = 24;
			if ( bloom > 0.15f || ctx.local_air ) budget = 36;
			if ( ctx.heavy && ctx.local_air ) budget = 44;
			if ( bloom > 0.35f ) budget += 8;
			constexpr float k_golden = 2.399963229728653f;
			for ( int i = 0; i < budget; ++i ) {
				const float t = ( static_cast<float>( i ) + 0.5f ) / static_cast<float>( budget );
				const float r = max_delta * std::sqrt( t );
				const float th = static_cast<float>( i ) * k_golden;
				math::vector3 c = wish;
				c.x += r * std::cos( th );
				c.y += r * std::sin( th );
				c.z = 0.0f;
				c.x = std::clamp( c.x, -89.0f, 89.0f );
				math::helpers::normalize_angle( c.y );
				math::vector3 wpy = wish; wpy.z = 0.0f;
				if ( detail::ang_delta( wpy, c ) > max_delta + 0.02f ) continue;

				shot probe{};
				if ( detail::try_view( eye, c, seed_tick, tick_frac, weapon, local_pawn, target, inac, spr,
					punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, probe, false ) ) {
					if ( detail::accept_view( eye, c, seed_tick, tick_frac, weapon, local_pawn, target,
						inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
						return true;
					continue;
				}
				math::vector3 d{};
				float sx = 0.0f, sy = 0.0f;
				if ( detail::bullet_dir_local( c, seed_tick, weapon, inac, spr, punch_ptr, d, &sx, &sy ) ) {
					math::vector3 rv{};
					if ( detail::roll_trick_view( c, punch_ptr, seed_tick, sx, sy, rv ) ) {
						if ( detail::accept_view( eye, rv, seed_tick, tick_frac, weapon, local_pawn, target,
							inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, wish, ctx, out ) )
							return true;
					}
				}
			}
		}

		// 5) Pure geometric + roll
		{
			auto pure = math::helpers::calculate_angle( eye, aim_pt );
			if ( punch_ptr ) { pure.x -= punch_ptr->x; pure.y -= punch_ptr->y; }
			pure.z = 0.0f;
			pure.x = std::clamp( pure.x, -89.0f, 89.0f );
			math::helpers::normalize_angle( pure.y );
			if ( hitchance::detail::is_valid_ang( pure ) ) {
				if ( detail::accept_view( eye, pure, seed_tick, tick_frac, weapon, local_pawn, target,
					inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, pure, ctx, out ) )
					return true;
				math::vector3 d{};
				float sx = 0.0f, sy = 0.0f;
				if ( detail::bullet_dir_local( pure, seed_tick, weapon, inac, spr, punch_ptr, d, &sx, &sy ) ) {
					math::vector3 rv{};
					if ( detail::roll_trick_view( pure, punch_ptr, seed_tick, sx, sy, rv ) ) {
						if ( detail::accept_view( eye, rv, seed_tick, tick_frac, weapon, local_pawn, target,
							inac, spr, punch_ptr, prefer_hb, enabled_hbs, max_delta, pure, ctx, out ) )
							return true;
					}
				}
			}
		}

		return out.ok;
	}

	[[nodiscard]] inline math::vector3 seed_aim_point( std::uintptr_t target, int hb_category, const math::vector3& fallback )
	{
		math::vector3 pt = fallback;
		if ( target && hb_category >= 0 && hb_category < hb_count ) {
			const int idx = hitchance::detail::pick_hitbox_for_category( target, hb_category );
			hitchance::detail::capsule cap{};
			if ( idx >= 0 && hitchance::detail::build_capsule( target, idx, cap ) )
				pt = cap.center;
		}
		if ( !target || !hitchance::detail::is_valid_vec( pt ) ) return pt;

		const auto vel = reinterpret_cast<C_BaseEntity*>( target )->m_vecAbsVelocity( );
		const float sp = vel.length( );
		if ( !std::isfinite( sp ) || sp < 30.0f ) return pt;

		const bool head = ( hb_category == hb_head || hb_category == hb_neck );
		const float lead_t = head ? 0.004f : 0.006f;
		math::vector3 lead{ pt.x + vel.x * lead_t, pt.y + vel.y * lead_t, pt.z + vel.z * lead_t };
		const float dx = lead.x - pt.x;
		const float dy = lead.y - pt.y;
		const float h = std::sqrt( dx * dx + dy * dy );
		const float max_h = head ? 3.0f : 5.0f;
		if ( h > max_h && h > 1e-4f ) {
			const float s = max_h / h;
			lead.x = pt.x + dx * s;
			lead.y = pt.y + dy * s;
		}
		const float dz = lead.z - pt.z;
		const float max_z = head ? 2.0f : 3.0f;
		if ( std::fabs( dz ) > max_z )
			lead.z = pt.z + ( dz > 0.0f ? max_z : -max_z );
		return hitchance::detail::is_valid_vec( lead ) ? lead : pt;
	}

	// Solve — top-level entry. Iterates HB categories in preference order.
	[[nodiscard]] inline bool solve( const math::vector3& eye, const math::vector3& wish_view,
		int seed_tick, float tick_frac, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		int prefer_hb, const bool* enabled_hbs, shot& out, float max_delta_floor = -1.0f )
	{
		out = {};
		if ( !ready( ) || seed_tick <= 0 ) return false;
		if ( !weapon || !local_pawn || !target ) return false;
		if ( !hitchance::detail::is_valid_vec( eye ) ) return false;
		if ( prefer_hb < 0 || prefer_hb >= hb_count ) prefer_hb = hb_head;

		float inac = 0.0f, spr = 0.0f;
		if ( !hitchance::read_current_bloom( weapon, local_pawn, inac, spr ) ) return false;

		math::vector3 punch{};
		const math::vector3* punch_ptr = nullptr;
		if ( hitchance::read_seed_fire_punch( local_pawn, weapon, seed_tick, tick_frac, punch ) ) {
			punch.z = 0.0f;
			punch_ptr = &punch;
		}
		else if ( hitchance::read_aim_punch( local_pawn, punch ) ) {
			punch.z = 0.0f;
			punch_ptr = &punch;
		}

		detail::try_ctx ctx{};
		detail::build_try_ctx( weapon, local_pawn, inac, spr, ctx );

		static constexpr int k_order[] = {
			hb_head, hb_neck, hb_chest, hb_stomach, hb_pelvis, hb_arms, hb_legs, hb_feet };

		int try_list[ hb_count ]{};
		int n_try = 0;
		auto push_hb = [ & ] ( int hb ) {
			if ( hb < 0 || hb >= hb_count ) return;
			if ( enabled_hbs && !enabled_hbs[ hb ] ) return;
			for ( int i = 0; i < n_try; ++i ) if ( try_list[ i ] == hb ) return;
			try_list[ n_try++ ] = hb;
			};
		push_hb( prefer_hb );
		for ( int hb : k_order ) push_hb( hb );
		if ( n_try == 0 ) {
			for ( int hb : k_order ) try_list[ n_try++ ] = hb;
		}

		math::vector3 wish_base = wish_view;
		wish_base.z = 0.0f;
		math::helpers::normalize_angle( wish_base.y );

		for ( int i = 0; i < n_try; ++i ) {
			const int hb = try_list[ i ];
			const int idx = hitchance::detail::pick_hitbox_for_category( target, hb );
			math::vector3 aim_pt{};
			hitchance::detail::capsule cap{};
			if ( idx >= 0 && hitchance::detail::build_capsule( target, idx, cap ) )
				aim_pt = cap.center;
			aim_pt = seed_aim_point( target, hb, aim_pt );
			if ( !hitchance::detail::is_valid_vec( aim_pt ) ) continue;

			shot cand{};
			if ( solve_for_aim( eye, wish_base, aim_pt, hb, seed_tick, tick_frac,
				weapon, local_pawn, target, inac, spr, punch_ptr, enabled_hbs, ctx, cand, max_delta_floor )
				&& cand.ok && hitchance::detail::is_valid_ang( cand.fire_angles ) ) {
				out = cand;
				return true;
			}
		}
		return out.ok;
	}

	// Direct algebraic spread compensation (no search).
	[[nodiscard]] inline bool direct_compensate( const math::vector3& /*eye*/, const math::vector3& wish_view,
		int seed_tick, float tick_frac, std::uintptr_t weapon, std::uintptr_t local_pawn, math::vector3& out_angles )
	{
		if ( seed_tick <= 0 || !weapon || !local_pawn ) return false;
		if ( !hitchance::spread_seed_ready( ) ) return false;

		float inac = 0.0f, spr = 0.0f;
		if ( !hitchance::read_current_bloom( weapon, local_pawn, inac, spr ) ) return false;

		math::vector3 punch{};
		bool punch_ok = hitchance::read_seed_fire_punch( local_pawn, weapon, seed_tick, tick_frac, punch );
		if ( !punch_ok ) punch_ok = hitchance::read_aim_punch( local_pawn, punch );
		if ( !punch_ok ) punch = {};

		math::vector3 punched = wish_view;
		punched.x += punch.x;
		punched.y += punch.y;
		punched.z = 0.0f;
		math::helpers::normalize_angle( punched.y );

		const std::uint32_t seed = hitchance::compute_seed( punched, seed_tick );

		math::vector3 dir{};
		float sx = 0.0f, sy = 0.0f;
		if ( !hitchance::get_bullet_direction_cached( wish_view, seed_tick, weapon, inac, spr,
			dir, &sx, &sy, 1u, &punch, false ) ) {
			if ( !hitchance::get_bullet_direction_cached( wish_view, seed_tick, weapon, inac, spr,
				dir, &sx, &sy, 1u, &punch, true ) )
				return false;
		}
		if ( !std::isfinite( sx ) || !std::isfinite( sy ) ) return false;

		const float magnitude = std::sqrt( sx * sx + sy * sy );
		out_angles = wish_view;
		if ( magnitude < 1e-7f ) {
			out_angles.z = 0.0f;
			return true;
		}
		const float pitch_comp = std::atan( magnitude ) * detail::k_rad_to_deg;
		const float roll_comp = std::atan2( sx, sy ) * detail::k_rad_to_deg;
		out_angles.x -= pitch_comp;
		out_angles.z = -roll_comp;
		out_angles.x = std::clamp( out_angles.x, -89.0f, 89.0f );
		math::helpers::normalize_angle( out_angles.y );

		math::vector3 verify_punched = out_angles;
		verify_punched.x += punch.x;
		verify_punched.y += punch.y;
		verify_punched.z = 0.0f;
		math::helpers::normalize_angle( verify_punched.y );
		if ( hitchance::compute_seed( verify_punched, seed_tick ) == seed )
			return true;

		const float orig_bin = detail::quant_half( punched.x );
		for ( int nudge = 0; nudge < 5; ++nudge ) {
			const float offset = ( nudge == 0 ) ? 0.0f :
				( ( nudge % 2 == 1 ) ? 0.25f * ( ( nudge + 1 ) / 2 ) : -0.25f * ( nudge / 2 ) );
			math::vector3 try_ang = out_angles;
			try_ang.x = wish_view.x - pitch_comp + offset;
			try_ang.x = std::clamp( try_ang.x, -89.0f, 89.0f );
			math::vector3 tp = try_ang;
			tp.x += punch.x; tp.y += punch.y; tp.z = 0.0f;
			math::helpers::normalize_angle( tp.y );
			if ( detail::quant_half( tp.x ) == orig_bin
				&& hitchance::compute_seed( tp, seed_tick ) == seed ) {
				out_angles.x = try_ang.x;
				math::helpers::normalize_angle( out_angles.y );
				return true;
			}
		}
		return false;
	}

	// Per-weapon soft-latch after fire.
	namespace detail {
		struct latch { std::uint16_t def{}; std::uint64_t fire_ms{}; };
		inline latch& get_latch( ) { static latch l{}; return l; }

		[[nodiscard]] inline std::uint64_t now_ms( )
		{
			return static_cast<std::uint64_t>( GetTickCount64( ) );
		}
	}

	[[nodiscard]] inline bool seed_cycle_allows_fire( std::uintptr_t weapon, std::uintptr_t /*local_pawn*/ )
	{
		if ( !weapon ) return true;
		auto& l = detail::get_latch( );
		const std::uint16_t def = hitchance::detail::weapon_def_idx( weapon );
		const std::uint64_t now = detail::now_ms( );
		const std::uint64_t gap = detail::is_heavy_pistol( weapon ) ? 8ull : ( detail::is_sniper( weapon ) ? 24ull : 6ull );

		if ( def != 0 && l.def == def && l.fire_ms != 0 && ( now - l.fire_ms ) < gap )
			return false;
		return true;
	}

	inline void note_seed_fired( std::uintptr_t weapon, std::uintptr_t /*local_pawn*/ = 0 )
	{
		if ( !weapon ) return;
		auto& l = detail::get_latch( );
		l.def = hitchance::detail::weapon_def_idx( weapon );
		l.fire_ms = detail::now_ms( );
	}

	[[nodiscard]] inline bool seed_passes_damage( const math::vector3& eye, math::vector3& inout_point,
		int& inout_hb_idx, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		bool allow_pen, float min_damage )
	{
		const float min_damage_vis = min_damage;
		const float min_damage_aw = min_damage;
		if ( !target || !weapon || !local_pawn ) return false;
		if ( !hitchance::detail::is_valid_vec( eye ) || !hitchance::detail::is_valid_vec( inout_point ) ) return false;
		if ( inout_hb_idx < 0 ) return false;

		int hb_cat = -1;
		int idx = inout_hb_idx;
		if ( inout_hb_idx >= 0 && inout_hb_idx < hb_count ) {
			hb_cat = inout_hb_idx;
			idx = hitchance::detail::pick_hitbox_for_category( target, inout_hb_idx );
		}

		hitchance::detail::capsule probe{};
		if ( idx < 0 || !hitchance::detail::build_capsule( target, idx, probe ) ) {
			if ( !hitchance::detail::build_capsule( target, inout_hb_idx, probe ) )
				return false;
			idx = inout_hb_idx;
		}

		auto& shared = features::combat::g_shared;
		auto ctx = shared.pen( ).prepare_target( target, nullptr );
		const int local_team = reinterpret_cast<C_BaseEntity*>( local_pawn )->m_iTeamNum( );
		const int target_hp = reinterpret_cast<C_BaseEntity*>( target )->m_iHealth( );

		auto need_for = [ & ]( float cfg_need ) -> float
			{
				if ( cfg_need <= 0.0f ) return 0.0f;
				float need = cfg_need;
				if ( target_hp > 0 && static_cast<float>( target_hp ) < need )
					need = static_cast<float>( target_hp );
				return need;
			};

		auto ok_damage = [ & ]( const math::vector3& pt, shared::penetration::result& out ) -> bool
			{
				out = {};
				if ( !shared.pen( ).run( eye, pt, ctx, local_pawn, local_team, out ) )
					return false;
				if ( out.damage < 1.0f )
					return false;

				if ( allow_pen ) {
					if ( !out.penetrated )
						return false;
					const float need = need_for( min_damage_aw );
					if ( need > 0.0f && out.damage + 0.01f < need )
						return false;
					return true;
				}

				if ( out.penetrated )
					return false;
				const float need = need_for( min_damage_vis );
				if ( need > 0.0f && out.damage + 0.01f < need )
					return false;
				return true;
			};

		shared::penetration::result res{};
		if ( ok_damage( inout_point, res ) ) {
			inout_hb_idx = ( hb_cat >= 0 ) ? hb_cat : idx;
			return true;
		}

		math::vector3 center = seed_aim_point( target, hb_cat >= 0 ? hb_cat : hitchance::hb_from_hitgroup( systems::g_hitboxes.hitgroup_from_hitbox( idx ) ), probe.center );
		if ( !hitchance::detail::is_valid_vec( center ) )
			center = probe.center;

		const float dx = center.x - inout_point.x;
		const float dy = center.y - inout_point.y;
		const float dz = center.z - inout_point.z;
		if ( ( dx * dx + dy * dy + dz * dz ) > 0.25f ) {
			shared::penetration::result res2{};
			if ( ok_damage( center, res2 ) ) {
				inout_point = center;
				inout_hb_idx = ( hb_cat >= 0 ) ? hb_cat : idx;
				return true;
			}
		}

		return false;
	}

	// Thin wrappers for callers
	[[nodiscard]] inline bool exact_shot_hits( const math::vector3& eye, const math::vector3& fire_angles,
		int seed_tick, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		int hitbox_idx, math::vector3* out_pt = nullptr, float tick_frac = 0.0f )
	{
		return hitchance::exact_shot_hits( eye, fire_angles, seed_tick, weapon, local_pawn, target, hitbox_idx, out_pt, tick_frac );
	}

	[[nodiscard]] inline bool exact_shot_hits_any( const math::vector3& eye, const math::vector3& fire_angles,
		int seed_tick, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target,
		const bool* enabled_hbs, int* out_hb = nullptr, math::vector3* out_pt = nullptr, float tick_frac = 0.0f )
	{
		return hitchance::exact_shot_hits_any( eye, fire_angles, seed_tick, weapon, local_pawn, target,
			enabled_hbs, out_hb, out_pt, tick_frac );
	}

}
