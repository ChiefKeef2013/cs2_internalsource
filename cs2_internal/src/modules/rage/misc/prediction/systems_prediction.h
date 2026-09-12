#pragma once

#include <core/common.hpp>
#include <core/memory.hpp>
#include <core/features.hpp>

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

	namespace detail {

		class state_guard
		{
		public:
			state_guard( )
			{
				this->m_entries.reserve( 96 );
				this->m_arena.reserve( 2048 );
			}

			~state_guard( ) { restore( ); }

			state_guard( const state_guard& ) = delete;
			state_guard& operator=( const state_guard& ) = delete;

			template <typename T>
			void save( std::uintptr_t address )
			{
				if ( !memory::detail::is_user_addr( address ) )
				{
					return;
				}

				const auto value = memory::read<T>( address );
				const auto offset = this->m_arena.size( );
				this->m_arena.resize( offset + sizeof( T ) );
				std::memcpy( this->m_arena.data( ) + offset, &value, sizeof( T ) );
				this->m_entries.push_back( entry{ address, offset, sizeof( T ) } );
			}

			void save_raw( std::uintptr_t address, std::size_t size )
			{
				if ( !memory::detail::is_user_addr( address ) || size == 0 || size > 0x400 )
				{
					return;
				}

				const auto offset = this->m_arena.size( );
				this->m_arena.resize( offset + size );
				std::memcpy( this->m_arena.data( ) + offset, reinterpret_cast< void* >( address ), size );
				this->m_entries.push_back( entry{ address, offset, size } );
			}

			void restore( )
			{
				for ( auto it = this->m_entries.rbegin( ); it != this->m_entries.rend( ); ++it )
				{
					if ( !memory::detail::is_user_addr( it->address ) || it->size == 0 )
					{
						continue;
					}

					std::memcpy( reinterpret_cast< void* >( it->address ), this->m_arena.data( ) + it->offset, it->size );
				}

				this->m_entries.clear( );
				this->m_arena.clear( );
			}

		private:
			struct entry
			{
				std::uintptr_t address{};
				std::size_t offset{};
				std::size_t size{};
			};

			std::vector<entry> m_entries;
			std::vector<std::uint8_t> m_arena;
		};

	}

	void prediction::capture_prestate( std::uintptr_t local_pawn, std::uintptr_t movement_services )
	{
		if ( !memory::is_game_ptr( local_pawn ) || !memory::is_game_ptr( movement_services ) )
		{
			return;
		}

		this->m_prestate.flags = reinterpret_cast<C_BaseEntity*>( local_pawn )->m_fFlags();
		this->m_prestate.networked_velocity = reinterpret_cast<C_BaseEntity*>( local_pawn )->m_vecVelocity();
		this->m_prestate.velocity = reinterpret_cast<C_BaseEntity*>( local_pawn )->m_vecAbsVelocity();
		this->m_prestate.stamina = reinterpret_cast<CCSPlayer_MovementServices*>( movement_services )->m_flStamina();
		this->m_prestate.surface_friction = reinterpret_cast<CPlayer_MovementServices_Humanoid*>( movement_services )->m_flSurfaceFriction();

		this->m_prestate.last_movement_impulses.x = reinterpret_cast<CPlayer_MovementServices*>( movement_services )->m_flCmdForwardMove();
		this->m_prestate.last_movement_impulses.y = reinterpret_cast<CPlayer_MovementServices*>( movement_services )->m_flCmdLeftMove();
		this->m_prestate.last_movement_impulses.z = reinterpret_cast<CPlayer_MovementServices*>( movement_services )->m_flCmdUpMove();

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( local_pawn )->m_pGameSceneNode();
		if ( memory::is_game_ptr( game_scene_node ) )
		{
			this->m_prestate.origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin();

			this->m_prestate.networked_origin = this->m_prestate.origin;
		}
	}

	bool prediction::simulate( input::usercmd* cmd, const systems::local::snapshot& local, const std::function<void( )>& fn )
	{
		std::lock_guard simulation_lock( this->m_simulation_mtx );

		static const auto prediction_set_state = PATTERN(PATTERN_PREDICTION_SET_STATE);
		static const auto prediction_set_pawn = PATTERN(PATTERN_PREDICTION_SET_PAWN);
		static const auto prediction_setup_move = PATTERN(PATTERN_PREDICTION_SETUP_MOVE);
		static const auto prediction_process_movement = PATTERN(PATTERN_PREDICTION_PROCESS_MOVEMENT);
		static const auto prediction_finish_move = PATTERN(PATTERN_PREDICTION_FINISH_MOVE);
		static const auto prediction_reset_pawn = PATTERN(PATTERN_PREDICTION_RESET_PAWN);

		if ( !prediction_set_state || !prediction_set_pawn || !prediction_setup_move ||
			!prediction_process_movement || !prediction_finish_move || !prediction_reset_pawn )
		{
			return false;
		}

		if ( systems::g_lifecycle.busy( ) || !cmd || !memory::is_game_ptr( local.pawn ) || !memory::is_game_ptr( local.controller ) )
		{
			return false;
		}

		const auto get_active_weapon = [ ]( std::uintptr_t pawn ) -> std::pair<std::uintptr_t, std::uintptr_t>
			{
				const auto weapon_services = reinterpret_cast<C_BasePlayerPawn*>( pawn )->m_pWeaponServices();
				if ( !weapon_services )
				{
					return {};
				}

				const auto weapon_handle = reinterpret_cast<CPlayer_WeaponServices*>( weapon_services )->m_hActiveWeapon();
				if ( !weapon_handle )
				{
					return { weapon_services, 0 };
				}

				return { weapon_services, systems::g_entities.lookup( weapon_handle ) };
			};

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( local.pawn )->m_pGameSceneNode();
		const auto movement_services = reinterpret_cast<C_BasePlayerPawn*>( local.pawn )->m_pMovementServices();
		const auto aim_punch_services = reinterpret_cast<C_CSPlayerPawn*>( local.pawn )->m_pAimPunchServices();
		const auto [weapon_services, weapon] = get_active_weapon( local.pawn );

		if ( !memory::is_game_ptr( game_scene_node ) || !memory::is_game_ptr( movement_services ) ||
			!memory::is_game_ptr( aim_punch_services ) || !memory::is_game_ptr( weapon_services ) ||
			!memory::is_game_ptr( weapon ) )
		{
			return false;
		}

		const auto global_vars = addresses::globals::global_vars
			? memory::read<std::uintptr_t>( addresses::globals::global_vars )
			: 0;
		const auto cmd_ptr = reinterpret_cast< std::uintptr_t >( cmd );
		const auto old_slot = addresses::globals::source2client_prediction
			? memory::read<std::uintptr_t>( addresses::globals::source2client_prediction + 56 )
			: 0;
		const auto pred_state = addresses::globals::prediction_state
			? memory::read<std::uintptr_t>( addresses::globals::prediction_state )
			: 0;

		if ( !global_vars || !pred_state )
		{
			return false;
		}

		const auto encoded_origin_offset = SCHEMA_OFFSET( "CGameSceneNode", "m_vecOrigin"_hash );
		const auto rotation_offset = SCHEMA_OFFSET( "CGameSceneNode", "m_angRotation"_hash );
		const auto encoded_origin_size = rotation_offset - encoded_origin_offset;
		if ( encoded_origin_offset <= 0 || encoded_origin_size <= 0 || encoded_origin_size > 0x80 )
		{
			return false;
		}

		detail::state_guard guard;

		guard.save<float>( global_vars + 48 );
		guard.save<float>( global_vars + 52 );
		guard.save<int>( global_vars + 68 );
		guard.save<float>( global_vars + 80 );
		guard.save<std::uint32_t>( global_vars + 88 );

		guard.save<std::uint32_t>( addresses::globals::prediction_seed );
		guard.save<std::uintptr_t>( addresses::globals::simulation_player );
		guard.save<std::uintptr_t>( addresses::globals::prediction_player );

		if ( old_slot )
		{
			guard.save<std::uint8_t>( old_slot + 140 );
		}

		guard.save<int>( local.controller + SCHEMA_OFFSET( "CBasePlayerController", "m_nTickBase"_hash ) );

		guard.save<std::uint32_t>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_fFlags"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_vecVelocity"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_vecBaseVelocity"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_flFriction"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_flGravityScale"_hash ) );
		guard.save<std::uint32_t>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_hGroundEntity"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_flSimulationTime"_hash ) );
		guard.save<int>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_nSimulationTick"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_BaseEntity", "m_flWaterLevel"_hash ) );

		guard.save<math::vector3>( game_scene_node + SCHEMA_OFFSET( "CGameSceneNode", "m_vecAbsOrigin"_hash ) );
		guard.save_raw( game_scene_node + encoded_origin_offset, static_cast< std::size_t >( encoded_origin_size ) );

		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_flVelocityModifier"_hash ) );
		guard.save<int>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_iShotsFired"_hash ) );
		guard.save<bool>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_bIsWalking"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_angEyeAngles"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_flLastFiredWeaponTime"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_ignoreLadderJumpTime"_hash ) );
		guard.save<float>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_grenadeParameterStashTime"_hash ) );
		guard.save<bool>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_bGrenadeParametersStashed"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_angStashedShootAngles"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_vecStashedGrenadeThrowPosition"_hash ) );
		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_CSPlayerPawn", "m_vecStashedVelocity"_hash ) );

		guard.save<math::vector3>( local.pawn + SCHEMA_OFFSET( "C_BaseModelEntity", "m_vecViewOffset"_hash ) );

		guard.save<int>( aim_punch_services + SCHEMA_OFFSET( "CCSPlayer_AimPunchServices", "m_predictableBaseTick"_hash ) );
		guard.save<float>( aim_punch_services + SCHEMA_OFFSET( "CCSPlayer_AimPunchServices", "m_predictableBaseTickInterpAmount"_hash ) );
		guard.save<math::vector3>( aim_punch_services + SCHEMA_OFFSET( "CCSPlayer_AimPunchServices", "m_predictableBaseAngle"_hash ) );
		guard.save<math::vector3>( aim_punch_services + SCHEMA_OFFSET( "CCSPlayer_AimPunchServices", "m_predictableBaseAngleVel"_hash ) );
		guard.save<int>( aim_punch_services + SCHEMA_OFFSET( "CCSPlayer_AimPunchServices", "m_unpredictableBaseTick"_hash ) );
		guard.save<math::vector3>( aim_punch_services + SCHEMA_OFFSET( "CCSPlayer_AimPunchServices", "m_unpredictableBaseAngle"_hash ) );

		guard.save<float>( weapon_services + SCHEMA_OFFSET( "CCSPlayer_WeaponServices", "m_flNextAttack"_hash ) );
		guard.save<std::uint32_t>( weapon_services + SCHEMA_OFFSET( "CCSPlayer_WeaponServices", "m_nOldTotalShootPositionHistoryCount"_hash ) );
		guard.save<std::uint32_t>( weapon_services + SCHEMA_OFFSET( "CCSPlayer_WeaponServices", "m_nOldTotalInputHistoryCount"_hash ) );

		guard.save<float>( weapon + SCHEMA_OFFSET( "C_CSWeaponBase", "m_flNextClientFireBulletTime"_hash ) );
		guard.save<float>( weapon + SCHEMA_OFFSET( "C_CSWeaponBase", "m_flNextClientFireBulletTime_Repredict"_hash ) );
		{
			const auto mode = SCHEMA_OFFSET( "C_CSWeaponBase", "m_weaponMode"_hash );
			const auto recoil = SCHEMA_OFFSET( "C_CSWeaponBase", "m_flRecoilIndex"_hash );
			if ( mode > 0 && recoil >= mode )
			{
				const auto size = static_cast< std::size_t >( recoil ) + sizeof( float ) - static_cast< std::size_t >( mode );
				if ( size && size <= 0x400 )
				{
					guard.save_raw( weapon + mode, size );
				}
			}

			const auto postpone = SCHEMA_OFFSET( "C_CSWeaponBase", "m_nPostponeFireReadyTicks"_hash );
			const auto hauled = SCHEMA_OFFSET( "C_CSWeaponBase", "m_bIsHauledBack"_hash );
			if ( postpone > 0 && hauled >= postpone )
			{
				const auto size = static_cast< std::size_t >( hauled ) + sizeof( bool ) - static_cast< std::size_t >( postpone );
				if ( size && size <= 0x400 )
				{
					guard.save_raw( weapon + postpone, size );
				}
			}
		}
		guard.save<float>( weapon + SCHEMA_OFFSET( "C_CSWeaponBase", "m_fLastShotTime"_hash ) );
		guard.save<float>( weapon + SCHEMA_OFFSET( "C_CSWeaponBase", "m_flNextAttackRenderTimeOffset"_hash ) );
		guard.save<float>( weapon + SCHEMA_OFFSET( "C_CSWeaponBase", "m_flWatTickOffset"_hash ) );
		guard.save<int>( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_nNextPrimaryAttackTick"_hash ) );
		guard.save<float>( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_flNextPrimaryAttackTickRatio"_hash ) );

		guard.save<int>( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_nNextSecondaryAttackTick"_hash ) );
		guard.save<float>( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_flNextSecondaryAttackTickRatio"_hash ) );
		guard.save<int>( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_iClip1"_hash ) );
		guard.save<int>( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_iClip2"_hash ) );
		guard.save_raw( weapon + SCHEMA_OFFSET( "C_BasePlayerWeapon", "m_pReserveAmmo"_hash ), sizeof( int ) * 2 );
		guard.save<int>( weapon + SCHEMA_OFFSET( "C_CSWeaponBaseGun", "m_zoomLevel"_hash ) );
		guard.save<int>( weapon + SCHEMA_OFFSET( "C_CSWeaponBaseGun", "m_iBurstShotsRemaining"_hash ) );

		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flStamina"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bDucked"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bDucking"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bDesiresDuck"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bDuckOverride"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flDuckAmount"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flDuckSpeed"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flDuckRootOffset"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flDuckViewOffset"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flLastDuckTime"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flBombPlantViewOffset"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bSpeedCropped"_hash ) );
		guard.save<int>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_nLadderSurfacePropIndex"_hash ) );
		guard.save<math::vector2>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_vecLastPositionAtFullCrouchSpeed"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_duckUntilOnGround"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bHasWalkMovedSinceLastJump"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bInStuckTest"_hash ) );
		guard.save<int>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_nOldWaterLevel"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flWaterEntryTime"_hash ) );
		guard.save<math::vector3>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_vecForward"_hash ) );
		guard.save<math::vector3>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_vecLeft"_hash ) );
		guard.save<math::vector3>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_vecUp"_hash ) );
		guard.save<int>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_nGameCodeHasMovedPlayerAfterCommand"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_fStashGrenadeParameterWhen"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flHeightAtJumpStart"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flMaxJumpHeightThisJump"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flMaxJumpHeightLastJump"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flStaminaAtJumpStart"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flVelMulAtJumpStart"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flAccumulatedJumpError"_hash ) );
		guard.save<math::vector2>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_vecWalkWishVel"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bHasEverProcessedCommand"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flTicksSinceLastSurfingDetected"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bJumpApexPending"_hash ) );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bWasSurfing"_hash ) );
		guard.save<int>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_nLastJumpTick"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flLastJumpFrac"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flLastJumpVelocityZ"_hash ) );
		guard.save_raw( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_LegacyJump"_hash ), 24 );
		guard.save_raw( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_ModernJump"_hash ), 56 );
		guard.save<bool>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_bUseFrictionStashedSpeed"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flUseFrictionStashedSpeedUntilFrac"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CCSPlayer_MovementServices", "m_flFrictionStashedSpeed"_hash ) );

		guard.save<float>( movement_services + SCHEMA_OFFSET( "CPlayer_MovementServices_Humanoid", "m_flSurfaceFriction"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CPlayer_MovementServices_Humanoid", "m_flFallVelocity"_hash ) );
		guard.save<math::vector3>( movement_services + SCHEMA_OFFSET( "CPlayer_MovementServices_Humanoid", "m_groundNormal"_hash ) );
		guard.save<float>( movement_services + SCHEMA_OFFSET( "CPlayer_MovementServices_Humanoid", "m_flStepSoundTime"_hash ) );
		guard.save<int>( movement_services + SCHEMA_OFFSET( "CPlayer_MovementServices_Humanoid", "m_nStepside"_hash ) );
		guard.save<std::uint32_t>( movement_services + SCHEMA_OFFSET( "CPlayer_MovementServices_Humanoid", "m_surfaceProps"_hash ) );
		guard.save<std::uintptr_t>( movement_services + 408 );

		{
			if ( old_slot )
			{
				memory::write<std::uint8_t>( old_slot + 140, 0 );
			}

			const auto next_tick = reinterpret_cast<CBasePlayerController*>( local.controller )->m_nTickBase() + 1;
			const auto next_time = static_cast< float >( next_tick ) * 0.015625f;

			memory::write<float>( global_vars + 48, next_time );
			memory::write<float>( global_vars + 52, 0.015625f );
			memory::write<int>( global_vars + 68, next_tick );
			memory::write<float>( global_vars + 80, 0.0f );
			memory::write<std::uint32_t>( global_vars + 88, GetCurrentThreadId( ) );

			memory::write( addresses::globals::simulation_player, local.pawn );
			memory::write( addresses::globals::prediction_player, local.pawn );
			reinterpret_cast<CBasePlayerController*>( local.controller )->m_nTickBase() = next_tick;

			std::uintptr_t pawn_guard[ 1 ]{};
			memory::call<void>( prediction_set_pawn, pawn_guard, local.pawn );
			memory::call<void>( prediction_set_state, pred_state, std::uint8_t( 1 ) );

			memory::call_vfunc<void>( movement_services, 46, cmd_ptr );

			const auto move_data = memory::call_vfunc<std::uintptr_t>( movement_services, 38 );
			const bool fb_seedsync = memory::call_vfunc<bool>( movement_services, 48 );

			if ( !move_data )
			{
				memory::call<void>( prediction_set_state, pred_state, std::uint8_t( 0 ) );
				memory::call<void>( prediction_reset_pawn, pawn_guard );
				memory::call_vfunc<void>( movement_services, 47 );
				return false;
			}

			memory::call<void>( prediction_setup_move, move_data, cmd_ptr, next_tick, fb_seedsync ? 1 : 0 );
			memory::call_vfunc<void>( movement_services, 39, cmd_ptr, move_data );
			memory::write<std::uintptr_t>( movement_services + 408, 0 );

			memory::call<void>( prediction_process_movement, movement_services, cmd_ptr, move_data, 1 );
			memory::call_vfunc<void>( movement_services, 43, cmd_ptr, move_data );
			memory::call<void>( prediction_finish_move, movement_services, cmd_ptr, move_data, 1 );

			memory::write( addresses::globals::simulation_player, 0ull );

			memory::call<void>( prediction_set_state, pred_state, std::uint8_t( 0 ) );
			memory::call<void>( prediction_reset_pawn, pawn_guard );
			memory::call_vfunc<void>( movement_services, 47 );

			fn( );
		}

		guard.restore( );

		return true;
	}

}





