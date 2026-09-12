#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/patterns.h>

namespace features::combat::hitchance {

	namespace detail {

		using fn_get_inaccuracy = float( __fastcall* )( std::uintptr_t weapon, float* out1, float* out2 );
		using fn_get_spread = float( __fastcall* )( std::uintptr_t weapon );
		using fn_seed_gen = std::uint32_t( __fastcall* )( void* unused, const math::vector3* angles, int attack_tick );
		using fn_calc_spread = void( __fastcall* )( std::uint16_t item_def, int n_bullets, int mode, std::uint32_t seed,
			float inac, float spread, float recoil_idx, float* out_x, float* out_y );
		using fn_update_turning = void( __fastcall* )( std::uintptr_t weapon );
		using fn_recovery_time = float( __fastcall* )( std::uintptr_t weapon );
		// client: mov rcx, [rcx+0x14B8] then punch helper — first arg is pawn, not AimPunchServices
		using fn_removed_aim_punch = void( __fastcall* )( std::uintptr_t pawn, math::vector3* out, char flag );
		using fn_compute_aim_punch_fire = void( __fastcall* )( std::uintptr_t services, math::vector3* out, void* fire_game_time, int flag );
		using fn_fill_gun_fire_data = void( __fastcall* )( std::uintptr_t weapon, void* out_fire_data, int mode );

		struct state
		{
			fn_get_inaccuracy get_inaccuracy{};
			fn_get_spread get_spread{};
			fn_seed_gen seed_gen{};
			fn_calc_spread calc_spread{};
			fn_update_turning update_turning{};
			fn_recovery_time recovery_time{};
			fn_removed_aim_punch removed_aim_punch{};
			fn_compute_aim_punch_fire compute_aim_punch_fire{};
			fn_fill_gun_fire_data fill_gun_fire_data{};
			bool initialized{};
			bool ready{};
		};

		inline state& get_state( )
		{
			static state s{};
			return s;
		}

		constexpr float k_two_pi = 6.28318530717958647692f;
		constexpr float k_rad_to_deg = 57.29577951308232f;
		constexpr float k_deg_to_rad = 0.017453292519943295f;
		constexpr std::uint32_t k_weapon_fire_punch_cache_off = 6940;

#pragma pack( push, 1 )
		struct gun_fire_data_raw
		{
			int tick;
			float frac;
			std::uint32_t unk8;
			int t12;
			float t16;
			float eye_x, eye_y, eye_z;
			float ang_x, ang_y, ang_z;
			float punch_x, punch_y, punch_z;
			std::uint8_t has_hist;
			std::uint8_t flag57;
			std::uint8_t pad58[ 2 ];
			int source_mode;
			std::uint8_t tail[ 64 ];
		};
#pragma pack( pop )

		[[nodiscard]] inline bool is_valid_vec( const math::vector3& v )
		{
			return std::isfinite( v.x ) && std::isfinite( v.y ) && std::isfinite( v.z );
		}

		[[nodiscard]] inline bool is_valid_ang( const math::vector3& a )
		{
			return std::isfinite( a.x ) && std::isfinite( a.y ) && std::isfinite( a.z )
				&& std::fabs( a.x ) < 3600.0f && std::fabs( a.y ) < 3600.0f;
		}

		inline void normalize_view( math::vector3& a )
		{
			math::helpers::normalize_angle( a.y );
			a.x = std::clamp( a.x, -89.0f, 89.0f );
		}

		struct valve_rng
		{
			int state = 0;
			int index = 0;
			int table[ 32 ]{};
			bool seeded = false;

			static int lcg( int v )
			{
				return 16807 * ( v % 127773 ) - 2836 * ( v / 127773 );
			}

			void seed( int s )
			{
				state = -std::abs( s );
				if ( state == 0 ) state = -1;
				index = 0;
				seeded = false;
			}

			int generate( )
			{
				if ( !seeded ) {
					int v = -state;
					if ( v < 1 ) v = 1;
					for ( int j = 39; j >= 0; --j ) {
						v = lcg( v );
						if ( v < 0 ) v += 2147483647;
						if ( j < 32 ) table[ j ] = v;
					}
					state = v;
					index = table[ 0 ];
					seeded = true;
				}
				state = lcg( state );
				if ( state < 0 ) state += 2147483647;
				const int idx = index / 0x4000000;
				index = table[ idx ];
				table[ idx ] = state;
				return index;
			}

			float random_float( float lo, float hi )
			{
				const float norm = std::min( 0.99999988f, static_cast<float>( generate( ) ) * 4.6566129e-10f );
				return lo + norm * ( hi - lo );
			}
		};

		inline void calc_spread_valve( std::uint32_t seed, float inac, float spr, float recoil_idx,
			std::uint16_t item_def, int mode, int n_bullets, float* out_x, float* out_y )
		{
			if ( !out_x || !out_y ) return;
			if ( n_bullets < 1 ) n_bullets = 1;
			if ( n_bullets > 16 ) n_bullets = 16;

			valve_rng rng;
			rng.seed( static_cast<int>( seed ) );

			for ( int b = 0; b < n_bullets; ++b ) {
				float inac_r = rng.random_float( 0.0f, 1.0f );
				const float inac_a = rng.random_float( 0.0f, k_two_pi );

				if ( item_def == 64 && mode == 1 )
					inac_r = 1.0f - ( inac_r * inac_r );
				else if ( item_def == 28 && recoil_idx < 3.0f ) {
					float v = inac_r; int c = 3;
					do { --c; v *= v; } while ( static_cast<float>( c ) > recoil_idx );
					inac_r = 1.0f - v;
				}
				inac_r *= inac;

				float spr_r = rng.random_float( 0.0f, 1.0f );
				const float spr_a = rng.random_float( 0.0f, k_two_pi );
				if ( item_def == 64 && mode == 1 )
					spr_r = 1.0f - ( spr_r * spr_r );
				else if ( item_def == 28 && recoil_idx < 3.0f ) {
					float v = spr_r; int c = 3;
					do { --c; v *= v; } while ( static_cast<float>( c ) > recoil_idx );
					spr_r = 1.0f - v;
				}
				spr_r *= spr;

				out_x[ b ] = std::cos( inac_a ) * inac_r + std::cos( spr_a ) * spr_r;
				out_y[ b ] = std::sin( inac_a ) * inac_r + std::sin( spr_a ) * spr_r;
			}
		}

		[[nodiscard]] inline std::uint16_t weapon_def_idx( std::uintptr_t weapon )
		{
			if ( !weapon ) return 0;
			return memory::read<std::uint16_t>( weapon
				+ SCHEMA_OFFSET( "C_EconEntity", "m_AttributeManager"_hash )
				+ SCHEMA_OFFSET( "C_AttributeContainer", "m_Item"_hash )
				+ SCHEMA_OFFSET( "C_EconItemView", "m_iItemDefinitionIndex"_hash ) );
		}

		[[nodiscard]] inline int weapon_bullet_count( std::uintptr_t weapon )
		{
			if ( !weapon ) return 1;
			const auto vdata = memory::read<std::uintptr_t>( weapon + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
			if ( !vdata ) return 1;
			int n = reinterpret_cast<CCSWeaponBaseVData*>( vdata )->m_nNumBullets( );
			if ( n < 1 ) n = 1;
			if ( n > 16 ) n = 16;
			return n;
		}

		[[nodiscard]] inline bool build_bullet_dir( const math::vector3& fire, float sx, float sy, math::vector3& out_dir )
		{
			math::vector3 forward{}, right{}, up{};
			fire.to_directions( &forward, &right, &up );
			out_dir = {
				forward.x - right.x * sx + up.x * sy,
				forward.y - right.y * sx + up.y * sy,
				forward.z - right.z * sx + up.z * sy
			};
			const float len_sqr = out_dir.length_sqr( );
			if ( !std::isfinite( len_sqr ) || len_sqr < 1e-12f ) return false;
			const float inv = 1.0f / std::sqrt( len_sqr );
			out_dir *= inv;
			return is_valid_vec( out_dir );
		}

		struct capsule
		{
			math::vector3 start{};
			math::vector3 end{};
			math::vector3 center{};
			float radius{};
			bool ok{};
		};

		[[nodiscard]] inline bool build_capsule( std::uintptr_t pawn, int hitbox_idx, capsule& out )
		{
			out = {};
			if ( !pawn || hitbox_idx < 0 ) return false;

			const auto gsn = reinterpret_cast<C_BaseEntity*>( pawn )->m_pGameSceneNode( );
			if ( !gsn ) return false;

			const auto set = systems::g_hitboxes.query( gsn );
			if ( set.count <= 0 ) return false;

			const systems::hitboxes::entry* found = nullptr;
			for ( int i = 0; i < set.count; ++i ) {
				if ( set.entries[ i ].index == hitbox_idx ) {
					found = &set.entries[ i ];
					break;
				}
			}
			if ( !found ) return false;

			const auto bone = systems::g_bones.get( pawn, static_cast<std::uint32_t>( found->bone ) );
			out.start = bone.rotation.rotate_vector( found->mins ) + bone.position;
			out.end = bone.rotation.rotate_vector( found->maxs ) + bone.position;
			out.center = ( out.start + out.end ) * 0.5f;
			out.radius = found->radius;
			out.ok = out.radius > 0.1f && is_valid_vec( out.center );
			return out.ok;
		}

		[[nodiscard]] inline bool capsule_hit( const math::vector3& eye, const math::vector3& dir, const capsule& cap, float scale )
		{
			if ( !cap.ok ) return false;
			float frac = 1.0f;
			const auto dir_scaled = dir * 8192.0f;
			const float r = cap.radius * scale;
			return features::combat::g_shared.ray_vs_capsule( eye, dir_scaled, cap.start, cap.end, r, frac );
		}

		[[nodiscard]] inline bool ray_hits_hitbox( std::uintptr_t pawn, int hitbox_idx,
			const math::vector3& eye, const math::vector3& dir, float scale,
			math::vector3* out_pt = nullptr )
		{
			capsule cap{};
			if ( !build_capsule( pawn, hitbox_idx, cap ) ) return false;
			float frac = 1.0f;
			const auto dir_scaled = dir * 8192.0f;
			const float r = cap.radius * scale;
			if ( !features::combat::g_shared.ray_vs_capsule( eye, dir_scaled, cap.start, cap.end, r, frac ) )
				return false;
			if ( out_pt ) *out_pt = eye + dir_scaled * frac;
			return true;
		}

	}

	[[nodiscard]] inline bool init( )
	{
		auto& s = detail::get_state( );
		if ( s.initialized ) return s.ready;
		s.initialized = true;

		s.get_inaccuracy = reinterpret_cast<detail::fn_get_inaccuracy>( PATTERN( PATTERN_SEED_GET_INACCURACY ) );
		if ( !s.get_inaccuracy )
			s.get_inaccuracy = reinterpret_cast<detail::fn_get_inaccuracy>( PATTERN( PATTERN_GET_INACCURACY ) );

		s.get_spread = reinterpret_cast<detail::fn_get_spread>( PATTERN( PATTERN_SEED_GET_SPREAD ) );
		if ( !s.get_spread )
			s.get_spread = reinterpret_cast<detail::fn_get_spread>( PATTERN( PATTERN_GET_SPREAD ) );

		s.seed_gen = reinterpret_cast<detail::fn_seed_gen>( PATTERN( PATTERN_SEED_SPREAD_SEED_GEN ) );
		if ( !s.seed_gen )
			s.seed_gen = reinterpret_cast<detail::fn_seed_gen>( PATTERN( PATTERN_GET_TICK_VIEW_ANGLES ) );

		s.calc_spread = reinterpret_cast<detail::fn_calc_spread>( PATTERN( PATTERN_SEED_CALC_SPREAD ) );
		if ( !s.calc_spread )
			s.calc_spread = reinterpret_cast<detail::fn_calc_spread>( PATTERN( PATTERN_WEAPON_CALCULATE_SPREAD ) );

		s.update_turning = reinterpret_cast<detail::fn_update_turning>( PATTERN( PATTERN_SEED_UPDATE_TURNING_INACCURACY ) );
		if ( !s.update_turning )
			s.update_turning = reinterpret_cast<detail::fn_update_turning>( PATTERN( PATTERN_WEAPON_UPDATE_ACCURACY ) );

		s.recovery_time = reinterpret_cast<detail::fn_recovery_time>( PATTERN( PATTERN_SEED_GET_INACCURACY_RECOVERY_TIME ) );
		s.removed_aim_punch = reinterpret_cast<detail::fn_removed_aim_punch>( PATTERN( PATTERN_SEED_GET_REMOVED_AIM_PUNCH ) );
		s.compute_aim_punch_fire = reinterpret_cast<detail::fn_compute_aim_punch_fire>( PATTERN( PATTERN_SEED_COMPUTE_AIM_PUNCH_FIRE ) );
		s.fill_gun_fire_data = reinterpret_cast<detail::fn_fill_gun_fire_data>( PATTERN( PATTERN_SEED_FILL_GUN_FIRE_DATA ) );

		s.ready = s.get_inaccuracy && s.get_spread && s.seed_gen;
		return s.ready;
	}

	[[nodiscard]] inline bool ready( )
	{
		return detail::get_state( ).ready;
	}

	[[nodiscard]] inline bool spread_seed_ready( )
	{
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );
		return s.seed_gen != nullptr;
	}

	struct gun_fire_data
	{
		int tick{};
		float frac{};
		math::vector3 eye{};
		math::vector3 angles{};
		math::vector3 punch{};
		int source_mode{ -1 };
		bool has_history{};
		bool ok{};
	};

	[[nodiscard]] inline bool fill_gun_fire_data( std::uintptr_t weapon, gun_fire_data& out, int mode = 0 )
	{
		out = {};
		if ( !weapon ) return false;
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );
		if ( !s.fill_gun_fire_data ) return false;

		alignas( 16 ) std::uint8_t saved_punch[ 12 ]{};
		const auto base = reinterpret_cast<std::uint8_t*>( weapon );
		std::memcpy( saved_punch, base + detail::k_weapon_fire_punch_cache_off, 12 );

		alignas( 16 ) detail::gun_fire_data_raw raw{};
		s.fill_gun_fire_data( weapon, &raw, mode );

		std::memcpy( base + detail::k_weapon_fire_punch_cache_off, saved_punch, 12 );

		out.tick = raw.tick;
		out.frac = std::isfinite( raw.frac ) ? std::clamp( raw.frac, 0.0f, 0.9999f ) : 0.0f;
		out.eye = math::vector3{ raw.eye_x, raw.eye_y, raw.eye_z };
		out.angles = math::vector3{ raw.ang_x, raw.ang_y, raw.ang_z };
		out.punch = math::vector3{ raw.punch_x, raw.punch_y, raw.punch_z };
		out.source_mode = raw.source_mode;
		out.has_history = raw.has_hist != 0;
		out.ok = out.tick > 0 && detail::is_valid_ang( out.angles ) && detail::is_valid_vec( out.eye );
		return out.ok;
	}

	[[nodiscard]] inline std::uint32_t compute_seed( const math::vector3& angles, int attack_tick )
	{
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );
		if ( !s.seed_gen ) return 0;
		math::vector3 a = angles;
		a.z = 0.0f;
		return s.seed_gen( nullptr, &a, attack_tick );
	}

	[[nodiscard]] inline bool read_aim_punch( std::uintptr_t local_pawn, math::vector3& out )
	{
		out = {};
		if ( !memory::is_game_ptr( local_pawn ) ) return false;
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );
		if ( !s.removed_aim_punch ) return false;
		// PATTERN_SEED_GET_REMOVED_AIM_PUNCH loads m_pAimPunchServices from the pawn itself.
		if ( !reinterpret_cast<C_CSPlayerPawn*>( local_pawn )->m_pAimPunchServices( ) ) return false;
		s.removed_aim_punch( local_pawn, &out, 1 );
		return detail::is_valid_ang( out );
	}

	[[nodiscard]] inline bool read_aim_punch_for_fire( std::uintptr_t local_pawn, int seed_tick, math::vector3& out, float tick_frac = 0.0f )
	{
		out = {};
		if ( !memory::is_game_ptr( local_pawn ) || seed_tick <= 0 ) return false;
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );

		math::vector3 fire_punch{};
		bool fire_ok = false;
		if ( s.compute_aim_punch_fire ) {
			const std::uintptr_t services = reinterpret_cast<C_CSPlayerPawn*>( local_pawn )->m_pAimPunchServices( );
			if ( memory::is_game_ptr( services ) ) {
				float frac = std::isfinite( tick_frac ) ? std::clamp( tick_frac, 0.0f, 0.9999f ) : 0.0f;
				alignas( 8 ) struct { int tick; float frac; } fire_time{ seed_tick, frac };
				s.compute_aim_punch_fire( services, &fire_punch, &fire_time, 1 );
				fire_ok = detail::is_valid_ang( fire_punch );
			}
		}

		math::vector3 cur_punch{};
		const bool cur_ok = read_aim_punch( local_pawn, cur_punch );
		const float fire_mag = fire_ok ? ( std::fabs( fire_punch.x ) + std::fabs( fire_punch.y ) ) : 0.0f;
		const float cur_mag = cur_ok ? ( std::fabs( cur_punch.x ) + std::fabs( cur_punch.y ) ) : 0.0f;

		if ( fire_ok && fire_mag > 0.02f ) { out = fire_punch; return true; }
		if ( cur_ok && cur_mag > fire_mag ) { out = cur_punch; return true; }
		if ( fire_ok ) { out = fire_punch; return true; }
		return cur_ok ? ( out = cur_punch, true ) : false;
	}

	[[nodiscard]] inline bool read_seed_fire_punch( std::uintptr_t local_pawn, std::uintptr_t weapon,
		int seed_tick, float tick_frac, math::vector3& out )
	{
		out = {};
		if ( weapon && seed_tick > 0 ) {
			gun_fire_data fd{};
			if ( fill_gun_fire_data( weapon, fd, 0 ) && detail::is_valid_ang( fd.punch ) && fd.tick > 0 ) {
				if ( std::abs( fd.tick - seed_tick ) <= 1 ) {
					out = fd.punch;
					out.z = 0.0f;
					return true;
				}
			}
		}
		return read_aim_punch_for_fire( local_pawn, seed_tick, out, tick_frac );
	}

	[[nodiscard]] inline bool read_current_bloom( std::uintptr_t weapon, std::uintptr_t /*local_pawn*/,
		float& out_inac, float& out_spr )
	{
		out_inac = -1.0f;
		out_spr = -1.0f;
		if ( !weapon ) return false;
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );

		if ( s.update_turning )
			s.update_turning( weapon );
		if ( s.get_inaccuracy ) {
			float o1 = 0.0f, o2 = 0.0f;
			out_inac = s.get_inaccuracy( weapon, &o1, &o2 );
		}
		if ( s.get_spread )
			out_spr = s.get_spread( weapon );

		if ( !std::isfinite( out_inac ) || out_inac < 0.0f ) {
			const float pen = reinterpret_cast<C_CSWeaponBase*>( weapon )->m_fAccuracyPenalty( );
			out_inac = std::isfinite( pen ) ? std::clamp( pen, 0.0f, 1.0f ) : 0.01f;
		}
		if ( !std::isfinite( out_spr ) || out_spr < 0.0f )
			out_spr = 0.004f;
		out_inac = std::clamp( out_inac, 0.0f, 1.0f );
		out_spr = std::clamp( out_spr, 0.0f, 1.0f );
		return std::isfinite( out_inac ) && std::isfinite( out_spr );
	}

	[[nodiscard]] inline bool calc_spread_game( std::uintptr_t weapon, std::uint32_t seed,
		float inac, float spr, int n_bullets, float* out_x, float* out_y )
	{
		auto& s = detail::get_state( );
		if ( !s.calc_spread || !weapon || !out_x || !out_y ) return false;
		if ( n_bullets < 1 ) n_bullets = 1;
		if ( n_bullets > 16 ) n_bullets = 16;
		const auto def = detail::weapon_def_idx( weapon );
		const int mode = static_cast<int>( reinterpret_cast<C_CSWeaponBase*>( weapon )->m_weaponMode( ) );
		const float recoil = reinterpret_cast<C_CSWeaponBase*>( weapon )->m_flRecoilIndex( );
		s.calc_spread( def, n_bullets, mode, seed, inac, spr, recoil, out_x, out_y );
		for ( int i = 0; i < n_bullets; ++i ) {
			if ( !std::isfinite( out_x[ i ] ) || !std::isfinite( out_y[ i ] ) ) return false;
		}
		return true;
	}

	inline void calc_spread_local( std::uintptr_t weapon, std::uint32_t seed, float inac, float spr,
		int n_bullets, float* out_x, float* out_y )
	{
		std::uint16_t def = 0;
		int mode = 0;
		float recoil = 0.0f;
		if ( weapon ) {
			def = detail::weapon_def_idx( weapon );
			mode = static_cast<int>( reinterpret_cast<C_CSWeaponBase*>( weapon )->m_weaponMode( ) );
			recoil = reinterpret_cast<C_CSWeaponBase*>( weapon )->m_flRecoilIndex( );
		}
		if ( !std::isfinite( recoil ) ) recoil = 0.0f;
		detail::calc_spread_valve( seed, inac, spr, recoil, def, mode, n_bullets, out_x, out_y );
	}

	[[nodiscard]] inline bool get_bullet_direction_cached( const math::vector3& fire_angles, int seed_tick,
		std::uintptr_t weapon, float inac, float spr, math::vector3& out_dir,
		float* out_sx, float* out_sy, unsigned seed_add,
		const math::vector3* aim_punch, bool use_local_spread )
	{
		out_dir = {};
		if ( !weapon || !detail::is_valid_ang( fire_angles ) || seed_tick <= 0 ) return false;
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );
		if ( !s.seed_gen ) return false;
		if ( !std::isfinite( inac ) || !std::isfinite( spr ) ) return false;

		math::vector3 ang = fire_angles;
		if ( aim_punch && detail::is_valid_ang( *aim_punch ) ) {
			ang.x += aim_punch->x;
			ang.y += aim_punch->y;
		}
		if ( !std::isfinite( ang.z ) ) ang.z = 0.0f;
		detail::normalize_view( ang );
		if ( !detail::is_valid_ang( ang ) ) return false;

		const std::uint32_t seed = compute_seed( ang, seed_tick );

		float sx = 0.0f, sy = 0.0f;
		if ( use_local_spread ) {
			float xs[ 16 ]{}, ys[ 16 ]{};
			calc_spread_local( weapon, seed + seed_add, inac, spr, 1, xs, ys );
			sx = xs[ 0 ]; sy = ys[ 0 ];
		}
		else {
			float xs[ 16 ]{}, ys[ 16 ]{};
			if ( !calc_spread_game( weapon, seed + seed_add, inac, spr, 1, xs, ys ) )
				calc_spread_local( weapon, seed + seed_add, inac, spr, 1, xs, ys );
			sx = xs[ 0 ]; sy = ys[ 0 ];
		}
		if ( !std::isfinite( sx ) || !std::isfinite( sy ) ) return false;

		if ( out_sx ) *out_sx = sx;
		if ( out_sy ) *out_sy = sy;
		return detail::build_bullet_dir( ang, sx, sy, out_dir );
	}

	[[nodiscard]] inline bool get_bullet_direction( const math::vector3& fire_angles, int seed_tick,
		std::uintptr_t weapon, std::uintptr_t local_pawn, math::vector3& out_dir,
		float* out_sx = nullptr, float* out_sy = nullptr, unsigned seed_add = 1u, float tick_frac = 0.0f )
	{
		float inac = 0.0f, spr = 0.0f;
		if ( !read_current_bloom( weapon, local_pawn, inac, spr ) ) return false;
		math::vector3 punch{};
		const math::vector3* punch_ptr = nullptr;
		if ( local_pawn && read_seed_fire_punch( local_pawn, weapon, seed_tick, tick_frac, punch ) )
			punch_ptr = &punch;
		return get_bullet_direction_cached( fire_angles, seed_tick, weapon, inac, spr,
			out_dir, out_sx, out_sy, seed_add, punch_ptr, false );
	}

	// Hitbox categories used by seed_trigger / nospread search
	enum : int
	{
		hb_head = 0,
		hb_neck = 1,
		hb_chest = 2,
		hb_stomach = 3,
		hb_pelvis = 4,
		hb_arms = 5,
		hb_legs = 6,
		hb_feet = 7,
		hb_count = 8
	};

	[[nodiscard]] inline int hb_from_hitgroup( int hg )
	{
		switch ( hg ) {
		case 1: return hb_head;
		case 8: return hb_neck;
		case 2: return hb_chest;
		case 3: return hb_stomach;
		case 4: case 5: return hb_arms;
		case 6: case 7: return hb_legs;
		default: return -1;
		}
	}

	namespace detail {

		struct hitbox_pick
		{
			int hitbox_idx{ -1 };
			int hb_category{ -1 };
		};

		[[nodiscard]] inline int pick_hitbox_for_category( std::uintptr_t pawn, int hb )
		{
			if ( !pawn || hb < 0 || hb >= hb_count ) return -1;
			const auto gsn = reinterpret_cast<C_BaseEntity*>( pawn )->m_pGameSceneNode( );
			if ( !gsn ) return -1;
			auto set = systems::g_hitboxes.query( gsn );
			int best = -1;
			for ( int i = 0; i < set.count; ++i ) {
				const auto hg = systems::g_hitboxes.hitgroup_from_hitbox( set.entries[ i ].index );
				const int cat = hb_from_hitgroup( hg );
				if ( cat != hb ) continue;
				if ( hb == hb_feet ) {
					if ( set.entries[ i ].index != 11 && set.entries[ i ].index != 12 ) continue;
				}
				best = set.entries[ i ].index;
				break;
			}
			return best;
		}

	}

	[[nodiscard]] inline bool exact_shot_hits( const math::vector3& eye, const math::vector3& fire_angles,
		int seed_tick, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target_pawn,
		int hitbox_idx, math::vector3* out_pt = nullptr, float tick_frac = 0.0f )
	{
		if ( out_pt ) *out_pt = {};
		if ( !target_pawn || hitbox_idx < 0 ) return false;
		if ( !detail::is_valid_vec( eye ) ) return false;

		float inac = 0.0f, spr = 0.0f;
		if ( !read_current_bloom( weapon, local_pawn, inac, spr ) ) return false;

		math::vector3 punch{};
		const math::vector3* punch_ptr = nullptr;
		if ( local_pawn && read_seed_fire_punch( local_pawn, weapon, seed_tick, tick_frac, punch ) )
			punch_ptr = &punch;

		math::vector3 ang = fire_angles;
		if ( punch_ptr ) { ang.x += punch_ptr->x; ang.y += punch_ptr->y; }
		if ( !std::isfinite( ang.z ) ) ang.z = 0.0f;
		detail::normalize_view( ang );
		if ( !detail::is_valid_ang( ang ) ) return false;

		const std::uint32_t seed = compute_seed( ang, seed_tick );
		const int n = detail::weapon_bullet_count( weapon );

		float xs[ 16 ]{}, ys[ 16 ]{};
		if ( !calc_spread_game( weapon, seed + 1u, inac, spr, n, xs, ys ) )
			calc_spread_local( weapon, seed + 1u, inac, spr, n, xs, ys );

		detail::capsule cap{};
		if ( !detail::build_capsule( target_pawn, hitbox_idx, cap ) ) return false;

		for ( int b = 0; b < n; ++b ) {
			if ( !std::isfinite( xs[ b ] ) || !std::isfinite( ys[ b ] ) ) continue;
			math::vector3 dir{};
			if ( !detail::build_bullet_dir( ang, xs[ b ], ys[ b ], dir ) ) continue;
			float frac = 1.0f;
			const auto dir_scaled = dir * 8192.0f;
			if ( !features::combat::g_shared.ray_vs_capsule( eye, dir_scaled, cap.start, cap.end, cap.radius, frac ) )
				continue;
			if ( out_pt ) *out_pt = eye + dir_scaled * frac;
			return true;
		}
		return false;
	}

	[[nodiscard]] inline bool exact_shot_hits_any( const math::vector3& eye, const math::vector3& fire_angles,
		int seed_tick, std::uintptr_t weapon, std::uintptr_t local_pawn, std::uintptr_t target_pawn,
		const bool* enabled_hbs, int* out_hb = nullptr, math::vector3* out_pt = nullptr, float tick_frac = 0.0f )
	{
		if ( out_hb ) *out_hb = -1;
		if ( out_pt ) *out_pt = {};
		if ( !target_pawn ) return false;

		static constexpr int k_order[] = { hb_head, hb_neck, hb_chest, hb_stomach, hb_pelvis, hb_arms, hb_legs, hb_feet };
		for ( int hb : k_order ) {
			if ( enabled_hbs && !enabled_hbs[ hb ] ) continue;
			const int idx = detail::pick_hitbox_for_category( target_pawn, hb );
			if ( idx < 0 ) continue;
			math::vector3 pt{};
			if ( exact_shot_hits( eye, fire_angles, seed_tick, weapon, local_pawn, target_pawn, idx, &pt, tick_frac ) ) {
				if ( out_hb ) *out_hb = hb;
				if ( out_pt ) *out_pt = pt;
				return true;
			}
		}
		return false;
	}

	// Monte-Carlo hitchance. required_pct 0 => always pass.
	[[nodiscard]] inline bool passes( const math::vector3& eye, const math::vector3& fire_angles,
		const math::vector3& aim_point, int hitbox_idx, std::uintptr_t weapon, float required_pct,
		std::uintptr_t local_pawn, std::uintptr_t target_pawn )
	{
		if ( !std::isfinite( required_pct ) ) return false;
		if ( required_pct <= 0.0f ) return true;
		if ( !weapon || !target_pawn ) return false;
		required_pct = std::clamp( required_pct, 0.0f, 100.0f );

		float inac = 0.0f, spr = 0.0f;
		if ( !read_current_bloom( weapon, local_pawn, inac, spr ) ) return false;

		detail::capsule cap{};
		if ( !detail::build_capsule( target_pawn, hitbox_idx, cap ) ) {
			return eye.distance( aim_point ) < 1.0f;
		}
		const float dist = eye.distance( cap.center );
		if ( dist < 1.0f ) return true;

		math::vector3 punched = fire_angles;
		math::vector3 punch{};
		if ( local_pawn && read_aim_punch( local_pawn, punch ) ) {
			punched.x += punch.x;
			punched.y += punch.y;
			punched.z = 0.0f;
			detail::normalize_view( punched );
		}

		math::vector3 forward{}, right{}, up{};
		punched.to_directions( &forward, &right, &up );

		const float bloom = inac + spr;
		int n_samples = 64;
		if ( required_pct >= 60.0f || bloom > 0.06f ) n_samples = 96;
		if ( required_pct >= 75.0f || bloom > 0.12f ) n_samples = 112;
		if ( required_pct >= 90.0f || bloom > 0.20f ) n_samples = 128;

		const int needed = std::max( 1, static_cast<int>(
			std::ceil( required_pct * static_cast<float>( n_samples ) / 100.0f + 1e-4f ) ) );

		const int n_bullets = detail::weapon_bullet_count( weapon );
		float xs[ 16 ]{}, ys[ 16 ]{};
		int hits = 0;
		for ( int i = 0; i < n_samples; ++i ) {
			calc_spread_local( weapon, static_cast<std::uint32_t>( i ), inac, spr, n_bullets, xs, ys );
			bool hit = false;
			for ( int b = 0; b < n_bullets && !hit; ++b ) {
				const float sx = xs[ b ], sy = ys[ b ];
				if ( !std::isfinite( sx ) || !std::isfinite( sy ) ) continue;
				math::vector3 dir{
					forward.x - right.x * sx + up.x * sy,
					forward.y - right.y * sx + up.y * sy,
					forward.z - right.z * sx + up.z * sy };
				const float len_sqr = dir.length_sqr( );
				if ( !std::isfinite( len_sqr ) || len_sqr < 1e-12f ) continue;
				dir *= ( 1.0f / std::sqrt( len_sqr ) );
				hit = detail::capsule_hit( eye, dir, cap, 1.0f );
			}
			if ( hit ) ++hits;
			const int left = n_samples - i - 1;
			if ( hits >= needed ) return true;
			if ( hits + left < needed ) return false;
		}
		return hits >= needed;
	}

	[[nodiscard]] inline float get_inaccuracy_recovery_time( std::uintptr_t weapon )
	{
		auto& s = detail::get_state( );
		if ( !s.initialized ) ( void )init( );
		if ( !s.recovery_time || !weapon ) return -1.0f;
		return s.recovery_time( weapon );
	}

}
