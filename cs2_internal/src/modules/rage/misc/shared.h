#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/common.hpp>
#include <modules/rage/misc/seed/hitchance.h>

namespace features::combat {

	void shared::shoot_history::snapshot( std::uintptr_t local_pawn, std::uintptr_t weapon_services )
	{
		this->m_count = 0;

		if ( !weapon_services )
		{
			return;
		}

		{
			const auto net_client = addresses::globals::network_client_service;
			if ( !net_client )
			{
				return;
			}

			const auto tick_state = memory::call_vfunc<std::uintptr_t>( net_client, cstypes::offsets::net_tick_info_vfunc );
			if ( !tick_state )
			{
				return;
			}

			this->m_server_tick = memory::read<int>( tick_state + cstypes::offsets::net_server_tick );
		}

		{
			const auto idx_raw = memory::read<int>( addresses::globals::frame_input_ring_idx );
			const auto idx = static_cast< unsigned >( idx_raw ) % 10u;
			const auto slot = addresses::globals::frame_input_ring_base + 40ull * idx;

			this->m_client_tick = memory::read<int>( slot + 0x0c );
			this->m_client_tick_frac = memory::read<float>( slot + 0x10 );
		}

		{
			const auto lerp_seconds = memory::call<float>(PATTERN(PATTERN_GET_INTERP_AMOUNT), local_pawn );
			const auto lerp_ticks_f = lerp_seconds * 64.0f;
			const auto rounded = std::round( lerp_ticks_f );

			if ( std::fabs( lerp_ticks_f - rounded ) < 1e-4f )
			{
				this->m_lerp_ticks_int = static_cast< int >( rounded );
				this->m_lerp_ticks_frac = 0.0f;
			}
			else
			{
				this->m_lerp_ticks_int = static_cast< int >( std::floor( lerp_ticks_f ) );
				this->m_lerp_ticks_frac = lerp_ticks_f - static_cast< float >( this->m_lerp_ticks_int );
			}
		}

		const auto tail = memory::read<int>( weapon_services + 872 );
		const auto count = memory::read<int>( weapon_services + 876 );

		if ( count <= 0 || count > 32 || tail < 0 || tail >= 32 )
		{
			return;
		}

		for ( auto i = 0; i < count; ++i )
		{
			const auto idx = ( tail + i ) % 32;
			const auto off = weapon_services + 232 + 20ull * idx;

			auto& e = this->m_entries[ i ];
			e.tick = memory::read<int>( off + 0x00 );
			e.fraction = memory::read<float>( off + 0x04 );
			e.position.x = memory::read<float>( off + 0x08 );
			e.position.y = memory::read<float>( off + 0x0C );
			e.position.z = memory::read<float>( off + 0x10 );
		}

		this->m_count = count;
	}

	shared::shoot_history::eye_candidates shared::shoot_history::get_candidates( ) const
	{
		eye_candidates out{};

		if ( this->m_count < 1 )
		{
			return out;
		}

		constexpr auto ring_slot{ 0.03125f };

		const auto newest_valid_tick = this->m_client_tick - this->m_lerp_ticks_int;
		const auto oldest_valid_tick = this->m_client_tick - this->m_lerp_ticks_int - 1;

		auto first_valid{ -1 };
		auto last_valid{ -1 };

		for ( auto i = 0; i < this->m_count; ++i )
		{
			const auto t = this->m_entries[ i ].tick;
			if ( t > newest_valid_tick || t < oldest_valid_tick )
			{
				continue;
			}

			if ( first_valid == -1 )
			{
				first_valid = i;
			}

			last_valid = i;
		}

		if ( last_valid == -1 )
		{
			return out;
		}

		const auto& newest = this->m_entries[ last_valid ];
		out.entries[ 0 ].position = newest.position;
		out.entries[ 0 ].player_tick = newest.tick;
		out.entries[ 0 ].player_frac = newest.fraction + ring_slot;
		out.entries[ 0 ].lerp_ticks_int = this->m_lerp_ticks_int;
		out.entries[ 0 ].lerp_ticks_frac = this->m_lerp_ticks_frac;
		out.count = 1;

		if ( first_valid != last_valid )
		{
			const auto& oldest = this->m_entries[ first_valid ];
			const auto  delta = oldest.position - newest.position;

			if ( delta.x * delta.x + delta.y * delta.y + delta.z * delta.z >= 4.0f )
			{
				out.entries[ 1 ].position = oldest.position;
				out.entries[ 1 ].player_tick = oldest.tick;
				out.entries[ 1 ].player_frac = oldest.fraction + ring_slot;
				out.entries[ 1 ].lerp_ticks_int = this->m_lerp_ticks_int;
				out.entries[ 1 ].lerp_ticks_frac = this->m_lerp_ticks_frac;
				out.count = 2;
			}
		}

		return out;
	}

	void shared::update( )
	{
		this->m_ctx = {};

		const auto local = systems::g_local.get( );
		if ( !local.pawn )
		{
			return;
		}

		const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );
		const auto movement_services = reinterpret_cast<C_BasePlayerPawn*>(local.pawn)->m_pMovementServices();

		if ( !global_vars || !movement_services )
		{
			return;
		}

		this->m_ctx.current_tick = memory::read<int>( global_vars + 0x44 );
		this->m_ctx.current_time = memory::read<float>( global_vars + 0x30 );
		this->m_ctx.is_scoped = systems::g_entities.is_cs_player_pawn( local.pawn )
			&& reinterpret_cast<C_CSPlayerPawn*>(local.pawn)->m_bIsScoped();
		this->m_ctx.ticks_since_land = this->m_ctx.current_tick - memory::read<int>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_ModernJump"_hash ) + SCHEMA_OFFSET( "CCSPlayerModernJump", "m_nLastLandedTick"_hash ) );
		this->m_ctx.weapon_services = reinterpret_cast<C_BasePlayerPawn*>(local.pawn)->m_pWeaponServices();

		if ( !this->m_ctx.weapon_services )
		{
			return;
		}

		const auto weapon_handle = reinterpret_cast<CPlayer_WeaponServices*>(this->m_ctx.weapon_services)->m_hActiveWeapon();
		if ( !weapon_handle )
		{
			return;
		}

		this->m_ctx.weapon = systems::g_entities.lookup( weapon_handle );
		if ( !this->m_ctx.weapon )
		{
			return;
		}

		this->m_ctx.weapon_vdata = memory::read<std::uintptr_t>( this->m_ctx.weapon + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
		if ( !this->m_ctx.weapon_vdata )
		{
			return;
		}

		const auto vdata = reinterpret_cast<CCSWeaponBaseVData*>(this->m_ctx.weapon_vdata);
		this->m_ctx.range = vdata->m_flRange();
		this->m_ctx.weapon_type = vdata->m_WeaponType();
		this->m_ctx.item_def_idx = memory::read<std::uint16_t>( this->m_ctx.weapon + SCHEMA_OFFSET( "C_EconEntity", "m_AttributeManager"_hash ) + SCHEMA_OFFSET( "C_AttributeContainer", "m_Item"_hash ) + SCHEMA_OFFSET( "C_EconItemView", "m_iItemDefinitionIndex"_hash ) );
		this->m_ctx.num_bullets = vdata->m_nNumBullets();
		this->m_ctx.weapon_mode = static_cast<int>( reinterpret_cast<C_CSWeaponBase*>( this->m_ctx.weapon )->m_weaponMode( ) );
		this->m_ctx.recoil_index = reinterpret_cast<C_CSWeaponBase*>(this->m_ctx.weapon)->m_flRecoilIndex();
		this->m_ctx.weapon_max_speed = vdata->m_flMaxSpeed().value();
		this->m_ctx.is_jump_scouting = ( systems::g_prediction.pre( ).flags & cstypes::entity_flags::on_ground ) == 0 && this->m_ctx.item_def_idx == cstypes::item_definition_index::weapon_ssg_08 && this->m_ctx.is_scoped;
		this->m_ctx.valid = true;

		this->m_pen.prepare( this->m_ctx.weapon_vdata, this->m_ctx.weapon );
	}

	void shared::invalidate_if_needed( )
	{
		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn || !local.controller )
		{
			this->m_ctx = {};
			this->m_last_shoot_tick = 0;
		}
	}

	std::uint32_t shared::get_spread_seed( const math::vector3& angles, int tick ) const
	{
		return memory::call<std::uint32_t>(PATTERN(PATTERN_GET_TICK_VIEW_ANGLES), nullptr, &angles, tick );
	}

	math::vector2 shared::calculate_spread( int seed, float accuracy, float spread, float recoil_index, int item_def_idx, int num_bullets ) const
	{
		math::vector2 out{};

		memory::call<void>(
			PATTERN( PATTERN_WEAPON_CALCULATE_SPREAD ),
			static_cast< std::int16_t >( item_def_idx ),
			num_bullets,
			0,
			static_cast< std::uint32_t >( seed + 1 ),
			accuracy,
			spread,
			recoil_index,
			&out.x,
			&out.y );

		return out;
	}

	math::vector3 shared::get_aim_punch( std::uintptr_t local_pawn ) const
	{
		math::vector3 out{};

		if ( !local_pawn )
		{
			return out;
		}

	if ( hitchance::read_aim_punch( local_pawn, out ) )
		{
			out.z = 0.0f;
			return out;
		}

		const auto services = reinterpret_cast<C_CSPlayerPawn*>( local_pawn )->m_pAimPunchServices( );
		if ( !services )
		{
			return out;
		}

		memory::call<void>( PATTERN( PATTERN_GET_AIM_PUNCH ), services, &out, 1u );
		out.z = 0.0f;
		return out;
	}

	float shared::calculate_hitchance( const math::vector3& shoot_position, const math::vector3& aim_angle, const systems::hitboxes::entry& hitbox, const systems::bones::data& bone, float inaccuracy, float spread, int samples ) const
	{
		const auto total = spread + inaccuracy;
		if ( total < 0.0001f )
		{
			return 1.0f;
		}

		if ( samples <= 0 )
		{
			return 0.0f;
		}

		if ( samples == 256 )
		{
			if ( total < 0.005f )
				samples = 64;
			else if ( total < 0.02f )
				samples = 128;
			else if ( total > 0.08f )
				samples = 192;
		}

		const auto capsule_start = bone.rotation.rotate_vector( hitbox.mins ) + bone.position;
		const auto capsule_end = bone.rotation.rotate_vector( hitbox.maxs ) + bone.position;
		const auto is_capsule = hitbox.radius > 0.001f;
		auto inverse_rotation = bone.rotation;
		inverse_rotation.x = -inverse_rotation.x;
		inverse_rotation.y = -inverse_rotation.y;
		inverse_rotation.z = -inverse_rotation.z;
		const auto box_ray_origin = inverse_rotation.rotate_vector( shoot_position - bone.position );

		const auto ray_vs_box = [ & ]( const math::vector3& ray_direction )
		{
			const auto direction = inverse_rotation.rotate_vector( ray_direction );
			auto entry{ 0.0f };
			auto exit{ 1.0f };

			const auto intersect_axis = [ & ]( float origin, float delta, float minimum, float maximum )
			{
			if ( std::fabs( delta ) < 1.0e-8f )
				{
					return origin >= minimum && origin <= maximum;
				}

				auto first = ( minimum - origin ) / delta;
				auto second = ( maximum - origin ) / delta;
				if ( first > second )
				{
					std::swap( first, second );
				}

				entry = std::max( entry, first );
				exit = std::min( exit, second );
				return entry <= exit;
			};

			return intersect_axis( box_ray_origin.x, direction.x, hitbox.mins.x, hitbox.maxs.x ) &&
				intersect_axis( box_ray_origin.y, direction.y, hitbox.mins.y, hitbox.maxs.y ) &&
				intersect_axis( box_ray_origin.z, direction.z, hitbox.mins.z, hitbox.maxs.z );
		};

		math::vector3 forward{}, left{}, up{};
	math::helpers::angle_vectors_left( aim_angle, &forward, &left, &up );

		struct spread_cache
		{
			float inaccuracy{};
			float spread{};
			float recoil_index{};
			int item_def_idx{};
			int weapon_mode{};
			int count{};
			bool initialized{};
			std::array<math::vector2, 256> values{};
		};

		thread_local spread_cache cache{};
		if ( !cache.initialized || cache.inaccuracy != inaccuracy || cache.spread != spread ||
			cache.recoil_index != this->m_ctx.recoil_index || cache.item_def_idx != this->m_ctx.item_def_idx ||
			cache.weapon_mode != this->m_ctx.weapon_mode )
		{
			cache.inaccuracy = inaccuracy;
			cache.spread = spread;
			cache.recoil_index = this->m_ctx.recoil_index;
			cache.item_def_idx = this->m_ctx.item_def_idx;
			cache.weapon_mode = this->m_ctx.weapon_mode;
			cache.count = 0;
			cache.initialized = true;
		}

		const auto sample_spread = [ & ]( int seed ) -> math::vector2
		{
			float sx{};
			float sy{};
			hitchance::detail::calc_spread_valve(
				static_cast< std::uint32_t >( seed + 1 ),
				inaccuracy,
				spread,
				this->m_ctx.recoil_index,
				static_cast< std::uint16_t >( this->m_ctx.item_def_idx ),
				this->m_ctx.weapon_mode,
				1,
				&sx,
				&sy );
			return { sx, sy };
		};

		const auto cached_samples = std::min( samples, static_cast< int >( cache.values.size( ) ) );
		for ( auto i = cache.count; i < cached_samples; ++i )
		{
			cache.values[ i ] = sample_spread( i );
		}
		cache.count = std::max( cache.count, cached_samples );

		auto hits{ 0 };

		for ( auto i = 0; i < samples; ++i )
		{
			const auto calculated_spread = i < cached_samples
				? cache.values[ i ]
				: sample_spread( i );
			const auto direction = forward + ( left * calculated_spread.x ) + ( up * calculated_spread.y );
			const auto ray_end = direction.normalized( ) * 8192.0f;

			auto hit{ false };
			if ( is_capsule )
			{
				auto fraction{ 1.0f };
				hit = this->ray_vs_capsule( shoot_position, ray_end, capsule_start, capsule_end, hitbox.radius, fraction );
			}
			else
			{
				hit = ray_vs_box( ray_end );
			}

			if ( hit )
			{
				++hits;
			}

			const auto remaining = samples - i - 1;
			if ( hits + remaining < static_cast< int >( samples * 0.1f ) )
			{
				break;
			}
		}

		return static_cast< float >( hits ) / static_cast< float >( samples );
	}

	math::vector3 shared::get_eye_position( std::uintptr_t local_pawn ) const
	{
		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>(local_pawn)->m_pGameSceneNode();
		const auto origin = reinterpret_cast<CGameSceneNode*>(game_scene_node)->m_vecAbsOrigin();
		const auto view_offset = reinterpret_cast<C_BaseModelEntity*>(local_pawn)->m_vecViewOffset();
		return origin + view_offset;
	}

	math::vector3 shared::get_shoot_position( ) const
	{
		math::vector3 out{};
		memory::call_vfunc<void>( this->m_ctx.weapon_services, 29, reinterpret_cast< std::uintptr_t >( &out ) );
		return out;
	}

	math::vector3 shared::get_interpolated_shoot_position( std::uintptr_t local_pawn, bool newest ) const
	{
		const auto ws = this->m_ctx.weapon_services;
		const auto head = memory::read<int>( ws + 872 );
		const auto count = memory::read<int>( ws + 876 );

		if ( count < 1 )
		{
			return this->get_shoot_position( );
		}

		if ( newest )
		{
			const auto newest_idx = ( head + count - 1 ) % 32;
			const auto newest_off = ws + 232 + 20ull * newest_idx;
			return memory::read<math::vector3>( newest_off + 8 );
		}

		if ( count < 2 )
		{
			return this->get_shoot_position( );
		}

		const auto interp = memory::call<float>(PATTERN(PATTERN_GET_INTERP_AMOUNT), local_pawn );
		const auto newest_idx = ( head + count - 1 ) % 32;
		const auto newest_off = ws + 232 + 20ull * newest_idx;
		const auto newest_tick = memory::read<int>( newest_off );
		const auto newest_frac = memory::read<float>( newest_off + 4 );

		if ( !std::isfinite( interp ) || interp <= 0.0f )
		{
			return memory::read<math::vector3>( newest_off + 8 );
		}

		const auto target = cstypes::tick_fraction{ newest_tick, newest_frac }.subtract_value( interp * 64.0f );

		for ( auto i = 0; i < count - 1; ++i )
		{
			const auto idx_a = ( head + static_cast< std::size_t >( i ) ) % 32;
			const auto idx_b = ( head + static_cast< std::size_t >( i ) + 1 ) % 32;

			const auto a_off = ws + 232 + 20ull * idx_a;
			const auto b_off = ws + 232 + 20ull * idx_b;

			const auto a_tick = memory::read<int>( a_off );
			const auto a_frac = memory::read<float>( a_off + 4 );
			const auto b_tick = memory::read<int>( b_off );
			const auto b_frac = memory::read<float>( b_off + 4 );

			const auto a_before = a_tick < target.tick || ( a_tick == target.tick && a_frac <= target.frac );
			if ( !a_before )
			{
				break;
			}

			const auto b_after = b_tick > target.tick || ( b_tick == target.tick && b_frac >= target.frac );
			if ( !b_after )
			{
				continue;
			}

			const auto a_pos = memory::read<math::vector3>( a_off + 8 );
			const auto b_pos = memory::read<math::vector3>( b_off + 8 );

			const auto span = cstypes::tick_fraction{ b_tick, b_frac }.subtract( { a_tick, a_frac } );
			const auto partial = target.subtract( { a_tick, a_frac } );

			const auto total_f = static_cast< float >( span.tick ) + span.frac;
			const auto partial_f = static_cast< float >( partial.tick ) + partial.frac;

			const auto t = total_f > 0.0f ? partial_f / total_f : 0.0f;

			return a_pos + ( b_pos - a_pos ) * t;
		}

		return this->get_shoot_position( );
	}

	float shared::get_spread( ) const
	{
		static const auto get_spread = PATTERN(PATTERN_GET_SPREAD);
		return memory::call<float>( get_spread, this->m_ctx.weapon );
	}

	float shared::get_inaccuracy( bool update_accuracy_penalty ) const
	{
		const auto accuracy_state_begin = SCHEMA_OFFSET( "C_CSWeaponBase", "m_flTurningInaccuracyDelta"_hash );
		const auto accuracy_state_end = SCHEMA_OFFSET( "C_CSWeaponBase", "m_flRecoilIndex"_hash );
		if ( !this->m_ctx.weapon || accuracy_state_begin <= 0 || accuracy_state_end < accuracy_state_begin )
		{
			return 0.0f;
		}

		const auto accuracy_state_size = static_cast< std::size_t >( accuracy_state_end - accuracy_state_begin ) + sizeof( float );
		if ( accuracy_state_size > 0x100 )
		{
			return 0.0f;
		}

		std::array<std::uint8_t, 0x100> backup{};
		std::memcpy( backup.data( ), reinterpret_cast< const void* >( this->m_ctx.weapon + accuracy_state_begin ), accuracy_state_size );

		if ( update_accuracy_penalty )
		{
			memory::call<void>(PATTERN(PATTERN_WEAPON_UPDATE_ACCURACY), this->m_ctx.weapon );
		}

		static const auto get_inaccuracy = PATTERN(PATTERN_GET_INACCURACY);
		const auto inaccuracy = memory::call<float>(
			get_inaccuracy, this->m_ctx.weapon,
			static_cast<float*>( nullptr ), static_cast<float*>( nullptr ) );

		std::memcpy( reinterpret_cast< void* >( this->m_ctx.weapon + accuracy_state_begin ), backup.data( ), accuracy_state_size );

		return inaccuracy;
	}

	float shared::get_inaccuracy_at_velocity( std::uintptr_t local_pawn, const math::vector3& velocity ) const
	{
		const auto accuracy_state_begin = SCHEMA_OFFSET( "C_CSWeaponBase", "m_flTurningInaccuracyDelta"_hash );
		const auto accuracy_state_end = SCHEMA_OFFSET( "C_CSWeaponBase", "m_flRecoilIndex"_hash );
		if ( !this->m_ctx.weapon || !local_pawn || accuracy_state_begin <= 0 || accuracy_state_end < accuracy_state_begin )
		{
			return 0.0f;
		}

		const auto accuracy_state_size = static_cast< std::size_t >( accuracy_state_end - accuracy_state_begin ) + sizeof( float );
		if ( accuracy_state_size > 0x100 )
		{
			return 0.0f;
		}

		std::array<std::uint8_t, 0x100> backup{};
		std::memcpy( backup.data( ), reinterpret_cast< const void* >( this->m_ctx.weapon + accuracy_state_begin ), accuracy_state_size );

		const auto pawn_ent = reinterpret_cast<C_BaseEntity*>(local_pawn);
		const auto old_velocity = pawn_ent->m_vecAbsVelocity();
		const auto old_eflags = pawn_ent->m_iEFlags();

		pawn_ent->m_iEFlags() = old_eflags & ~0x1000u;
		pawn_ent->m_vecAbsVelocity() = velocity;

		memory::call<void>(PATTERN(PATTERN_WEAPON_UPDATE_ACCURACY), this->m_ctx.weapon );

		static const auto get_inaccuracy = PATTERN(PATTERN_GET_INACCURACY);
		const auto inaccuracy = memory::call<float>(
			get_inaccuracy, this->m_ctx.weapon,
			static_cast<float*>( nullptr ), static_cast<float*>( nullptr ) );

		pawn_ent->m_vecAbsVelocity() = old_velocity;
		pawn_ent->m_iEFlags() = old_eflags;

		std::memcpy( reinterpret_cast< void* >( this->m_ctx.weapon + accuracy_state_begin ), backup.data( ), accuracy_state_size );

		return inaccuracy;
	}

	float shared::get_air_inaccuracy( float vertical_speed, float jump_initial, float jump_apex ) const
	{
		constexpr auto sqrt_threshold{ 17.37795666f };
		const auto val = ( ( std::sqrtf( std::fabsf( vertical_speed ) ) - sqrt_threshold * 0.25f ) * ( jump_initial - jump_apex ) ) / ( sqrt_threshold * 0.75f ) + jump_apex;
		return std::clamp( val, 0.0f, jump_initial * 2.0f );
	}

	bool shared::can_shoot( systems::input::usercmd* cmd, std::uintptr_t local_controller, bool check_next_attack ) const
	{
		if ( this->m_ctx.weapon_type != cstypes::weapon_type::knife )
		{
			if ( reinterpret_cast<C_CSWeaponBase*>(this->m_ctx.weapon)->m_bInReload() )
			{
				return false;
			}

			if ( reinterpret_cast<C_BasePlayerWeapon*>(this->m_ctx.weapon)->m_iClip1() <= 0 )
			{
				return false;
			}
		}

		if ( !check_next_attack )
		{
			return true;
		}

		const auto tick_base = reinterpret_cast<CBasePlayerController*>(local_controller)->m_nTickBase();
		const auto base_cmd = cmd->csgo_user_cmd.base( );
		const auto client_tick = base_cmd ? base_cmd->client_tick( ) : tick_base;
		const auto next_primary = reinterpret_cast<C_BasePlayerWeapon*>(this->m_ctx.weapon)->m_nNextPrimaryAttackTick();

		if ( this->m_ctx.weapon_type == cstypes::weapon_type::knife )
		{
			const auto next_secondary = reinterpret_cast<C_BasePlayerWeapon*>(this->m_ctx.weapon)->m_nNextSecondaryAttackTick();
			return tick_base > this->m_last_shoot_tick && ( client_tick >= next_primary || client_tick >= next_secondary );
		}

		return tick_base > this->m_last_shoot_tick && client_tick >= next_primary;
	}

	bool shared::is_max_accuracy( float inaccuracy ) const
	{
		const auto& prestate = systems::g_prediction.pre( );
		const auto on_ground = ( prestate.flags & 1 ) != 0;
		const auto is_ducking = ( prestate.flags & 4 ) != 0;
		const auto speed = prestate.networked_velocity.length_2d( );

		const auto nospread = CONVAR( "weapon_accuracy_nospread" );
		if ( nospread && nospread->get<bool>( ) )
		{
			return true;
		}

		const auto idx = this->m_ctx.item_def_idx;
		const auto is_scope_weapon =
			idx == cstypes::item_definition_index::weapon_ssg_08 ||
			idx == cstypes::item_definition_index::weapon_awp ||
			idx == cstypes::item_definition_index::weapon_scar_20 ||
			idx == cstypes::item_definition_index::weapon_g3sg1 ||
			idx == cstypes::item_definition_index::weapon_aug ||
			idx == cstypes::item_definition_index::weapon_sg_553;

		if ( is_scope_weapon )
		{
			const auto local_pawn = systems::g_local.get( ).pawn;
			const auto camera = local_pawn ? memory::read<std::uintptr_t>( local_pawn + SCHEMA( "C_BasePlayerPawn", "m_pCameraServices"_hash ) ) : 0;
			if ( camera )
			{
				auto scope_inac = memory::read<math::vector3>( camera + SCHEMA( "CCSPlayer_CameraServices", "m_vClientScopeInaccuracy"_hash ) );
				if ( scope_inac.x <= 1e-6f )
				{
					scope_inac.x = 0.0f;
				}

				switch ( idx )
				{
				case cstypes::item_definition_index::weapon_ssg_08:
					if ( scope_inac.x <= 0.00089f ) { return true; }
					break;
				case cstypes::item_definition_index::weapon_awp:
					if ( scope_inac.x <= 0.0005f ) { return true; }
					break;
				case cstypes::item_definition_index::weapon_scar_20:
				case cstypes::item_definition_index::weapon_g3sg1:
					if ( scope_inac.x <= 0.0012f ) { return true; }
					break;
				case cstypes::item_definition_index::weapon_aug:
				case cstypes::item_definition_index::weapon_sg_553:
					if ( scope_inac.x <= 0.0025f ) { return true; }
					break;
				default:
					break;
				}
			}
		}

		if ( idx == cstypes::item_definition_index::weapon_r8_revolver )
		{
			return inaccuracy <= 0.005f;
		}

		if ( on_ground )
		{
			if ( this->m_ctx.weapon_type == cstypes::weapon_type::sniper )
			{
				if ( !this->m_ctx.is_scoped )
				{
					return false;
				}

				if ( is_ducking )
				{
					const auto rounded = std::floorf( inaccuracy * 300.0f ) / 300.0f;
					return rounded < inaccuracy;
				}

				if ( speed <= 0.1f )
				{
					const auto rounded = std::floorf( inaccuracy * 170.0f ) / 170.0f;
					return rounded < inaccuracy;
				}

				return false;
			}

			return speed <= this->m_ctx.weapon_max_speed * 0.34f;
		}

		const auto inaccuracy_jump_apex = reinterpret_cast<CCSWeaponBaseVData*>(this->m_ctx.weapon_vdata)->m_flInaccuracyJumpApex();
		const auto accuracy_penalty = reinterpret_cast<C_CSWeaponBase*>(this->m_ctx.weapon)->m_fAccuracyPenalty();
		const auto min_air_inaccuracy = accuracy_penalty + inaccuracy_jump_apex;

		constexpr auto tolerance{ 0.001f };
		return inaccuracy <= min_air_inaccuracy + tolerance;
	}

	math::vector3 shared::simulate_aim_punch( int recoil_index ) const
	{
		if ( recoil_index <= 0 || !this->m_ctx.valid )
		{
			return {};
		}

		const auto weapon_mode = reinterpret_cast<C_CSWeaponBase*>(this->m_ctx.weapon)->m_weaponMode();
		const auto cycle_time = reinterpret_cast<CCSWeaponBaseVData*>(this->m_ctx.weapon_vdata)->m_flCycleTime().value();

		constexpr auto decay_rate{ 4.5f };
		constexpr auto decay2_exp{ 8.0f };
		constexpr auto decay2_lin{ 18.0f };
		constexpr auto recoil_scale{ 2.0f };

		math::vector3 punch{};
		math::vector3 punch_vel{};

		auto hybrid_decay = [ ]( math::vector3& v, float exp, float lin, float dt )
			{
				v *= std::expf( -exp * dt );

				const auto mag = v.length( );
				if ( mag > lin * dt )
				{
					v *= ( 1.0f - ( lin * dt ) / mag );
				}
				else
				{
					v = {};
				}
			};

		for ( auto i = 0; i < recoil_index; ++i )
		{
			float angle{}, magnitude{};
			memory::call<void>(PATTERN(PATTERN_WEAPON_GET_RECOIL_OFFSET), addresses::globals::weapon_recoil_data, this->m_ctx.weapon, weapon_mode, i, &angle, &magnitude );

			math::vector3 offset{};
			offset.x = std::cosf( math::helpers::deg_to_rad( angle ) ) * magnitude;
			offset.y = std::sinf( math::helpers::deg_to_rad( angle ) ) * magnitude;

			punch_vel -= offset;

			for ( auto time = 0.0f; time <= cycle_time; time += cstypes::tick_interval )
			{
				hybrid_decay( punch, decay2_exp, decay2_lin, cstypes::tick_interval );

				punch += punch_vel * cstypes::tick_interval * 0.5f;
				punch_vel *= std::expf( -decay_rate * cstypes::tick_interval );

				if ( punch_vel.length( ) < 0.03125f )
				{
					punch_vel = {};
				}

				punch += punch_vel * cstypes::tick_interval * 0.5f;
			}
		}

		return punch * recoil_scale;
	}

	bool shared::ray_vs_capsule( const math::vector3& ray_origin, const math::vector3& ray_dir, const math::vector3& capsule_a, const math::vector3& capsule_b, float radius, float& out_fraction ) const
	{
		const auto ab = capsule_b - capsule_a;
		const auto ab_sq = ab.dot( ab );
		const auto oc = ray_origin - capsule_a;
		const auto dir_sq = ray_dir.dot( ray_dir );

		if ( dir_sq < 1e-8f )
		{
			return false;
		}

		auto best_t{ 1.0f };
		auto hit{ false };

		if ( ab_sq > 1e-8f )
		{
			const float m = ab.dot( ray_dir ) / ab_sq;
			const float n = ab.dot( oc ) / ab_sq;

			const auto d_perp = ray_dir - ab * m;
			const auto oc_perp = oc - ab * n;

			const auto a = d_perp.dot( d_perp );
			const auto half_b = d_perp.dot( oc_perp );
			const auto c = oc_perp.dot( oc_perp ) - radius * radius;

			if ( a > 1e-8f )
			{
				const auto disc = half_b * half_b - a * c;
				if ( disc >= 0.0f )
				{
					const auto sqrt_disc = std::sqrt( disc );

					for ( int r = 0; r < 2; r++ )
					{
						const auto t = ( -half_b + ( r == 0 ? -sqrt_disc : sqrt_disc ) ) / a;
						if ( t < 0.0f || t >= best_t )
						{
							continue;
						}

						const auto s = m * t + n;
						if ( s >= 0.0f && s <= 1.0f )
						{
							best_t = t;
							hit = true;
							break;
						}
					}
				}
			}
		}

		const math::vector3 caps[ ]{ capsule_a, capsule_b };

		for ( int i = 0; i < 2; i++ )
		{
			const auto co = ray_origin - caps[ i ];
			const auto half_b = co.dot( ray_dir );
			const auto c = co.dot( co ) - radius * radius;
			const auto disc = half_b * half_b - dir_sq * c;

			if ( disc < 0.0f )
			{
				continue;
			}

			const auto sqrt_disc = std::sqrt( disc );

			for ( int r = 0; r < 2; r++ )
			{
				const auto t = ( -half_b + ( r == 0 ? -sqrt_disc : sqrt_disc ) ) / dir_sq;
				if ( t < 0.0f || t >= best_t )
				{
					continue;
				}

				if ( ab_sq > 1e-8f )
				{
					const auto hit_point = ray_origin + ray_dir * t - caps[ i ];
					const auto sign = i == 0 ? -1.0f : 1.0f;

					if ( sign * ab.dot( hit_point ) < 0.0f )
					{
						continue;
					}
				}

				best_t = t;
				hit = true;
				break;
			}
		}

		if ( hit )
		{
			out_fraction = best_t;
		}

		return hit;
	}

}



