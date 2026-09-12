#pragma once

class C_BasePlayerWeapon;
class C_CS2HudModelArms;
class C_EconItemView;
class CCSPlayerLegacyJump;

class CCSPlayer_AimPunchServices
{
public:
	SCHEMA_FIELD(math::vector3, m_predictableBaseAngle, "client.dll", "CCSPlayer_AimPunchServices", "m_predictableBaseAngle")
	SCHEMA_FIELD(math::vector3, m_predictableBaseAngleVel, "client.dll", "CCSPlayer_AimPunchServices", "m_predictableBaseAngleVel")
	SCHEMA_FIELD(GameTick_t, m_predictableBaseTick, "client.dll", "CCSPlayer_AimPunchServices", "m_predictableBaseTick")
	SCHEMA_FIELD(float, m_predictableBaseTickInterpAmount, "client.dll", "CCSPlayer_AimPunchServices", "m_predictableBaseTickInterpAmount")
	SCHEMA_FIELD(math::vector3, m_unpredictableBaseAngle, "client.dll", "CCSPlayer_AimPunchServices", "m_unpredictableBaseAngle")
	SCHEMA_FIELD(GameTick_t, m_unpredictableBaseTick, "client.dll", "CCSPlayer_AimPunchServices", "m_unpredictableBaseTick")
};

class CPlayer_ObserverServices
{
public:
	SCHEMA_FIELD(std::uint8_t, m_iObserverMode, "client.dll", "CPlayer_ObserverServices", "m_iObserverMode")
	SCHEMA_FIELD(CHandle<C_BaseEntity>, m_hObserverTarget, "client.dll", "CPlayer_ObserverServices", "m_hObserverTarget")
};

class CPlayer_MovementServices
{
public:
	SCHEMA_FIELD(float, m_flMaxspeed, "client.dll", "CPlayer_MovementServices", "m_flMaxspeed")
	SCHEMA_FIELD(float, m_flCmdForwardMove, "client.dll", "CPlayer_MovementServices", "m_flCmdForwardMove")
	SCHEMA_FIELD(float, m_flCmdLeftMove, "client.dll", "CPlayer_MovementServices", "m_flCmdLeftMove")
	SCHEMA_FIELD(float, m_flCmdUpMove, "client.dll", "CPlayer_MovementServices", "m_flCmdUpMove")
	SCHEMA_FIELD(std::uint32_t, m_nLastCommandNumberProcessed, "client.dll", "CPlayer_MovementServices", "m_nLastCommandNumberProcessed")
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
public:
	SCHEMA_FIELD(float, m_flSurfaceFriction, "client.dll", "CPlayer_MovementServices_Humanoid", "m_flSurfaceFriction")
	SCHEMA_FIELD(float, m_flFallVelocity, "client.dll", "CPlayer_MovementServices_Humanoid", "m_flFallVelocity")
	SCHEMA_FIELD(float, m_flStepSoundTime, "client.dll", "CPlayer_MovementServices_Humanoid", "m_flStepSoundTime")
	SCHEMA_FIELD(math::vector3, m_groundNormal, "client.dll", "CPlayer_MovementServices_Humanoid", "m_groundNormal")
	SCHEMA_FIELD(std::int32_t, m_nStepside, "client.dll", "CPlayer_MovementServices_Humanoid", "m_nStepside")
	SCHEMA_FIELD(CUtlStringToken, m_surfaceProps, "client.dll", "CPlayer_MovementServices_Humanoid", "m_surfaceProps")
};

class CCSPlayerModernJump
{
public:
	SCHEMA_FIELD(float, m_flLastLandedVelocityZ, "client.dll", "CCSPlayerModernJump", "m_flLastLandedVelocityZ")
	SCHEMA_FIELD(GameTick_t, m_nLastLandedTick, "client.dll", "CCSPlayerModernJump", "m_nLastLandedTick")
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
public:
	SCHEMA_FIELD(float, m_flDuckAmount, "client.dll", "CCSPlayer_MovementServices", "m_flDuckAmount")
	SCHEMA_FIELD(float, m_flDuckSpeed, "client.dll", "CCSPlayer_MovementServices", "m_flDuckSpeed")
	SCHEMA_FIELD(float, m_flStamina, "client.dll", "CCSPlayer_MovementServices", "m_flStamina")
	SCHEMA_FIELD(CCSPlayerModernJump, m_ModernJump, "client.dll", "CCSPlayer_MovementServices", "m_ModernJump")
	SCHEMA_FIELD(CCSPlayerLegacyJump, m_LegacyJump, "client.dll", "CCSPlayer_MovementServices", "m_LegacyJump")
	SCHEMA_FIELD(bool, m_bDesiresDuck, "client.dll", "CCSPlayer_MovementServices", "m_bDesiresDuck")
	SCHEMA_FIELD(bool, m_bDuckOverride, "client.dll", "CCSPlayer_MovementServices", "m_bDuckOverride")
	SCHEMA_FIELD(bool, m_bDucked, "client.dll", "CCSPlayer_MovementServices", "m_bDucked")
	SCHEMA_FIELD(bool, m_bDucking, "client.dll", "CCSPlayer_MovementServices", "m_bDucking")
	SCHEMA_FIELD(bool, m_bHasEverProcessedCommand, "client.dll", "CCSPlayer_MovementServices", "m_bHasEverProcessedCommand")
	SCHEMA_FIELD(bool, m_bHasWalkMovedSinceLastJump, "client.dll", "CCSPlayer_MovementServices", "m_bHasWalkMovedSinceLastJump")
	SCHEMA_FIELD(bool, m_bInStuckTest, "client.dll", "CCSPlayer_MovementServices", "m_bInStuckTest")
	SCHEMA_FIELD(bool, m_bJumpApexPending, "client.dll", "CCSPlayer_MovementServices", "m_bJumpApexPending")
	SCHEMA_FIELD(bool, m_bSpeedCropped, "client.dll", "CCSPlayer_MovementServices", "m_bSpeedCropped")
	SCHEMA_FIELD(bool, m_bUseFrictionStashedSpeed, "client.dll", "CCSPlayer_MovementServices", "m_bUseFrictionStashedSpeed")
	SCHEMA_FIELD(bool, m_bWasSurfing, "client.dll", "CCSPlayer_MovementServices", "m_bWasSurfing")
	SCHEMA_FIELD(bool, m_duckUntilOnGround, "client.dll", "CCSPlayer_MovementServices", "m_duckUntilOnGround")
	SCHEMA_FIELD(GameTime_t, m_fStashGrenadeParameterWhen, "client.dll", "CCSPlayer_MovementServices", "m_fStashGrenadeParameterWhen")
	SCHEMA_FIELD(float, m_flAccumulatedJumpError, "client.dll", "CCSPlayer_MovementServices", "m_flAccumulatedJumpError")
	SCHEMA_FIELD(float, m_flBombPlantViewOffset, "client.dll", "CCSPlayer_MovementServices", "m_flBombPlantViewOffset")
	SCHEMA_FIELD(float, m_flDuckRootOffset, "client.dll", "CCSPlayer_MovementServices", "m_flDuckRootOffset")
	SCHEMA_FIELD(float, m_flDuckViewOffset, "client.dll", "CCSPlayer_MovementServices", "m_flDuckViewOffset")
	SCHEMA_FIELD(float, m_flFrictionStashedSpeed, "client.dll", "CCSPlayer_MovementServices", "m_flFrictionStashedSpeed")
	SCHEMA_FIELD(float, m_flHeightAtJumpStart, "client.dll", "CCSPlayer_MovementServices", "m_flHeightAtJumpStart")
	SCHEMA_FIELD(float, m_flLastDuckTime, "client.dll", "CCSPlayer_MovementServices", "m_flLastDuckTime")
	SCHEMA_FIELD(float, m_flLastJumpFrac, "client.dll", "CCSPlayer_MovementServices", "m_flLastJumpFrac")
	SCHEMA_FIELD(float, m_flLastJumpVelocityZ, "client.dll", "CCSPlayer_MovementServices", "m_flLastJumpVelocityZ")
	SCHEMA_FIELD(float, m_flMaxJumpHeightLastJump, "client.dll", "CCSPlayer_MovementServices", "m_flMaxJumpHeightLastJump")
	SCHEMA_FIELD(float, m_flMaxJumpHeightThisJump, "client.dll", "CCSPlayer_MovementServices", "m_flMaxJumpHeightThisJump")
	SCHEMA_FIELD(float, m_flStaminaAtJumpStart, "client.dll", "CCSPlayer_MovementServices", "m_flStaminaAtJumpStart")
	SCHEMA_FIELD(float, m_flTicksSinceLastSurfingDetected, "client.dll", "CCSPlayer_MovementServices", "m_flTicksSinceLastSurfingDetected")
	SCHEMA_FIELD(float, m_flUseFrictionStashedSpeedUntilFrac, "client.dll", "CCSPlayer_MovementServices", "m_flUseFrictionStashedSpeedUntilFrac")
	SCHEMA_FIELD(float, m_flVelMulAtJumpStart, "client.dll", "CCSPlayer_MovementServices", "m_flVelMulAtJumpStart")
	SCHEMA_FIELD(float, m_flWaterEntryTime, "client.dll", "CCSPlayer_MovementServices", "m_flWaterEntryTime")
	SCHEMA_FIELD(std::int32_t, m_nGameCodeHasMovedPlayerAfterCommand, "client.dll", "CCSPlayer_MovementServices", "m_nGameCodeHasMovedPlayerAfterCommand")
	SCHEMA_FIELD(std::int32_t, m_nLadderSurfacePropIndex, "client.dll", "CCSPlayer_MovementServices", "m_nLadderSurfacePropIndex")
	SCHEMA_FIELD(GameTick_t, m_nLastJumpTick, "client.dll", "CCSPlayer_MovementServices", "m_nLastJumpTick")
	SCHEMA_FIELD(std::int32_t, m_nOldWaterLevel, "client.dll", "CCSPlayer_MovementServices", "m_nOldWaterLevel")
	SCHEMA_FIELD(math::vector3, m_vecForward, "client.dll", "CCSPlayer_MovementServices", "m_vecForward")
	SCHEMA_FIELD(math::vector3, m_vecLeft, "client.dll", "CCSPlayer_MovementServices", "m_vecLeft")
	SCHEMA_FIELD(math::vector3, m_vecUp, "client.dll", "CCSPlayer_MovementServices", "m_vecUp")
	SCHEMA_FIELD(math::vector2, m_vecLastPositionAtFullCrouchSpeed, "client.dll", "CCSPlayer_MovementServices", "m_vecLastPositionAtFullCrouchSpeed")
	SCHEMA_FIELD(math::vector2, m_vecWalkWishVel, "client.dll", "CCSPlayer_MovementServices", "m_vecWalkWishVel")
};

class CPlayer_WeaponServices
{
public:
	SCHEMA_FIELD(CHandle<C_BasePlayerWeapon>, m_hActiveWeapon, "client.dll", "CPlayer_WeaponServices", "m_hActiveWeapon")
	SCHEMA_FIELD(C_NetworkUtlVectorBase<CHandle<C_BasePlayerWeapon>>, m_hMyWeapons, "client.dll", "CPlayer_WeaponServices", "m_hMyWeapons")
};

class CCSPlayer_WeaponServices : public CPlayer_WeaponServices
{
public:
	SCHEMA_FIELD(GameTime_t, m_flNextAttack, "client.dll", "CCSPlayer_WeaponServices", "m_flNextAttack")
	SCHEMA_FIELD(std::uint32_t, m_nOldTotalInputHistoryCount, "client.dll", "CCSPlayer_WeaponServices", "m_nOldTotalInputHistoryCount")
	SCHEMA_FIELD(std::uint32_t, m_nOldTotalShootPositionHistoryCount, "client.dll", "CCSPlayer_WeaponServices", "m_nOldTotalShootPositionHistoryCount")
};

class CCSPlayer_ItemServices
{
public:
	SCHEMA_FIELD(bool, m_bHasDefuser, "client.dll", "CCSPlayer_ItemServices", "m_bHasDefuser")
	SCHEMA_FIELD(bool, m_bHasHelmet, "client.dll", "CCSPlayer_ItemServices", "m_bHasHelmet")
};

class C_BasePlayerPawn : public C_BaseModelEntity
{
public:
	SCHEMA_FIELD(std::uintptr_t, m_pWeaponServices, "client.dll", "C_BasePlayerPawn", "m_pWeaponServices")
	SCHEMA_FIELD(std::uintptr_t, m_pItemServices, "client.dll", "C_BasePlayerPawn", "m_pItemServices")
	SCHEMA_FIELD(std::uintptr_t, m_pObserverServices, "client.dll", "C_BasePlayerPawn", "m_pObserverServices")
	SCHEMA_FIELD(std::uintptr_t, m_pMovementServices, "client.dll", "C_BasePlayerPawn", "m_pMovementServices")
	SCHEMA_FIELD(CHandle<>, m_hController, "client.dll", "C_BasePlayerPawn", "m_hController")
	SCHEMA_FIELD(float, m_flFOVSensitivityAdjust, "client.dll", "C_BasePlayerPawn", "m_flFOVSensitivityAdjust")
};

class EntitySpottedState_t
{
public:
	SCHEMA_FIELD(bool, m_bSpotted, "client.dll", "EntitySpottedState_t", "m_bSpotted")
};

class C_CSPlayerPawnBase : public C_BasePlayerPawn
{
public:
	SCHEMA_FIELD(float, m_flFlashBangTime, "client.dll", "C_CSPlayerPawnBase", "m_flFlashBangTime")
	SCHEMA_FIELD(float, m_flFlashMaxAlpha, "client.dll", "C_CSPlayerPawnBase", "m_flFlashMaxAlpha")
	SCHEMA_FIELD(GameTime_t, m_flLastSpawnTimeIndex, "client.dll", "C_CSPlayerPawnBase", "m_flLastSpawnTimeIndex")
};

class C_CSPlayerPawn : public C_CSPlayerPawnBase
{
public:
	SCHEMA_FIELD(std::int32_t, m_ArmorValue, "client.dll", "C_CSPlayerPawn", "m_ArmorValue")
	SCHEMA_FIELD(math::vector3, m_angEyeAngles, "client.dll", "C_CSPlayerPawn", "m_angEyeAngles")
	SCHEMA_FIELD(bool, m_bGunGameImmunity, "client.dll", "C_CSPlayerPawn", "m_bGunGameImmunity")
	SCHEMA_FIELD(bool, m_bIsDefusing, "client.dll", "C_CSPlayerPawn", "m_bIsDefusing")
	SCHEMA_FIELD(bool, m_bIsScoped, "client.dll", "C_CSPlayerPawn", "m_bIsScoped")
	SCHEMA_FIELD(bool, m_bIsWalking, "client.dll", "C_CSPlayerPawn", "m_bIsWalking")
	SCHEMA_FIELD(bool, m_bNeedToReApplyGloves, "client.dll", "C_CSPlayerPawn", "m_bNeedToReApplyGloves")
	SCHEMA_FIELD(CHandle<C_CS2HudModelArms>, m_hHudModelArms, "client.dll", "C_CSPlayerPawn", "m_hHudModelArms")
	SCHEMA_FIELD(std::int32_t, m_iShotsFired, "client.dll", "C_CSPlayerPawn", "m_iShotsFired")
	SCHEMA_FIELD(std::uintptr_t, m_pAimPunchServices, "client.dll", "C_CSPlayerPawn", "m_pAimPunchServices")
	SCHEMA_FIELD(EntitySpottedState_t, m_entitySpottedState, "client.dll", "C_CSPlayerPawn", "m_entitySpottedState")
	SCHEMA_FIELD(C_EconItemView, m_EconGloves, "client.dll", "C_CSPlayerPawn", "m_EconGloves")
	SCHEMA_FIELD(std::uint8_t, m_nEconGlovesChanged, "client.dll", "C_CSPlayerPawn", "m_nEconGlovesChanged")
	SCHEMA_FIELD(math::vector3, m_angStashedShootAngles, "client.dll", "C_CSPlayerPawn", "m_angStashedShootAngles")
	SCHEMA_FIELD(bool, m_bGrenadeParametersStashed, "client.dll", "C_CSPlayerPawn", "m_bGrenadeParametersStashed")
	SCHEMA_FIELD(GameTime_t, m_flLastFiredWeaponTime, "client.dll", "C_CSPlayerPawn", "m_flLastFiredWeaponTime")
	SCHEMA_FIELD(float, m_flVelocityModifier, "client.dll", "C_CSPlayerPawn", "m_flVelocityModifier")
	SCHEMA_FIELD(GameTime_t, m_grenadeParameterStashTime, "client.dll", "C_CSPlayerPawn", "m_grenadeParameterStashTime")
	SCHEMA_FIELD(float, m_ignoreLadderJumpTime, "client.dll", "C_CSPlayerPawn", "m_ignoreLadderJumpTime")
	SCHEMA_FIELD(math::vector3, m_vecStashedGrenadeThrowPosition, "client.dll", "C_CSPlayerPawn", "m_vecStashedGrenadeThrowPosition")
	SCHEMA_FIELD(math::vector3, m_vecStashedVelocity, "client.dll", "C_CSPlayerPawn", "m_vecStashedVelocity")
};
