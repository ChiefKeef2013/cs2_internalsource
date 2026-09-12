#pragma once

class CBasePlayerWeaponVData
{
public:
	SCHEMA_FIELD(std::int32_t, m_iMaxClip1, "client.dll", "CBasePlayerWeaponVData", "m_iMaxClip1")
};

class CCSWeaponBaseVData : public CBasePlayerWeaponVData
{
public:
	SCHEMA_FIELD(CSWeaponType, m_WeaponType, "client.dll", "CCSWeaponBaseVData", "m_WeaponType")
	SCHEMA_FIELD(float, m_flArmorRatio, "client.dll", "CCSWeaponBaseVData", "m_flArmorRatio")
	SCHEMA_FIELD(CFiringModeFloat, m_flCycleTime, "client.dll", "CCSWeaponBaseVData", "m_flCycleTime")
	SCHEMA_FIELD(float, m_flHeadshotMultiplier, "client.dll", "CCSWeaponBaseVData", "m_flHeadshotMultiplier")
	SCHEMA_FIELD(float, m_flInaccuracyJumpApex, "client.dll", "CCSWeaponBaseVData", "m_flInaccuracyJumpApex")
	SCHEMA_FIELD(float, m_flInaccuracyJumpInitial, "client.dll", "CCSWeaponBaseVData", "m_flInaccuracyJumpInitial")
	SCHEMA_FIELD(CFiringModeFloat, m_flInaccuracyMove, "client.dll", "CCSWeaponBaseVData", "m_flInaccuracyMove")
	SCHEMA_FIELD(CFiringModeFloat, m_flInaccuracyStand, "client.dll", "CCSWeaponBaseVData", "m_flInaccuracyStand")
	SCHEMA_FIELD(CFiringModeFloat, m_flMaxSpeed, "client.dll", "CCSWeaponBaseVData", "m_flMaxSpeed")
	SCHEMA_FIELD(float, m_flPenetration, "client.dll", "CCSWeaponBaseVData", "m_flPenetration")
	SCHEMA_FIELD(float, m_flRange, "client.dll", "CCSWeaponBaseVData", "m_flRange")
	SCHEMA_FIELD(float, m_flRangeModifier, "client.dll", "CCSWeaponBaseVData", "m_flRangeModifier")
	SCHEMA_FIELD(float, m_flThrowVelocity, "client.dll", "CCSWeaponBaseVData", "m_flThrowVelocity")
	SCHEMA_FIELD(std::int32_t, m_nDamage, "client.dll", "CCSWeaponBaseVData", "m_nDamage")
	SCHEMA_FIELD(std::int32_t, m_nNumBullets, "client.dll", "CCSWeaponBaseVData", "m_nNumBullets")
	SCHEMA_FIELD(CGlobalSymbol, m_szName, "client.dll", "CCSWeaponBaseVData", "m_szName")
};

class C_BasePlayerWeapon : public C_BaseEntity
{
public:
	SCHEMA_FIELD(std::int32_t, m_iClip1, "client.dll", "C_BasePlayerWeapon", "m_iClip1")
	SCHEMA_FIELD(std::int32_t, m_iClip2, "client.dll", "C_BasePlayerWeapon", "m_iClip2")
	SCHEMA_FIELD(GameTick_t, m_nNextPrimaryAttackTick, "client.dll", "C_BasePlayerWeapon", "m_nNextPrimaryAttackTick")
	SCHEMA_FIELD(float, m_flNextPrimaryAttackTickRatio, "client.dll", "C_BasePlayerWeapon", "m_flNextPrimaryAttackTickRatio")
	SCHEMA_FIELD(GameTick_t, m_nNextSecondaryAttackTick, "client.dll", "C_BasePlayerWeapon", "m_nNextSecondaryAttackTick")
	SCHEMA_FIELD(float, m_flNextSecondaryAttackTickRatio, "client.dll", "C_BasePlayerWeapon", "m_flNextSecondaryAttackTickRatio")
	SCHEMA_ARRAY(std::int32_t, m_pReserveAmmo, 2, "client.dll", "C_BasePlayerWeapon", "m_pReserveAmmo")
};

class C_CSWeaponBase : public C_BasePlayerWeapon
{
public:
	SCHEMA_FIELD(bool, m_bVisualsDataSet, "client.dll", "C_CSWeaponBase", "m_bVisualsDataSet")
	SCHEMA_FIELD(bool, m_bInReload, "client.dll", "C_CSWeaponBase", "m_bInReload")
	SCHEMA_FIELD(float, m_fAccuracyPenalty, "client.dll", "C_CSWeaponBase", "m_fAccuracyPenalty")
	SCHEMA_FIELD(GameTime_t, m_fLastShotTime, "client.dll", "C_CSWeaponBase", "m_fLastShotTime")
	SCHEMA_FIELD(float, m_flRecoilIndex, "client.dll", "C_CSWeaponBase", "m_flRecoilIndex")
	SCHEMA_FIELD(float, m_flTurningInaccuracy, "client.dll", "C_CSWeaponBase", "m_flTurningInaccuracy")
	SCHEMA_FIELD(float, m_flTurningInaccuracyDelta, "client.dll", "C_CSWeaponBase", "m_flTurningInaccuracyDelta")
	SCHEMA_FIELD(CSWeaponMode, m_weaponMode, "client.dll", "C_CSWeaponBase", "m_weaponMode")
	SCHEMA_FIELD(bool, m_bIsHauledBack, "client.dll", "C_CSWeaponBase", "m_bIsHauledBack")
	SCHEMA_FIELD(float, m_flNextAttackRenderTimeOffset, "client.dll", "C_CSWeaponBase", "m_flNextAttackRenderTimeOffset")
	SCHEMA_FIELD(float, m_flNextClientFireBulletTime, "client.dll", "C_CSWeaponBase", "m_flNextClientFireBulletTime")
	SCHEMA_FIELD(float, m_flNextClientFireBulletTime_Repredict, "client.dll", "C_CSWeaponBase", "m_flNextClientFireBulletTime_Repredict")
	SCHEMA_FIELD(float, m_flWatTickOffset, "client.dll", "C_CSWeaponBase", "m_flWatTickOffset")
	SCHEMA_FIELD(GameTick_t, m_nPostponeFireReadyTicks, "client.dll", "C_CSWeaponBase", "m_nPostponeFireReadyTicks")
};

class C_CSWeaponBaseGun : public C_CSWeaponBase
{
public:
	SCHEMA_FIELD(std::int32_t, m_iBurstShotsRemaining, "client.dll", "C_CSWeaponBaseGun", "m_iBurstShotsRemaining")
	SCHEMA_FIELD(std::int32_t, m_zoomLevel, "client.dll", "C_CSWeaponBaseGun", "m_zoomLevel")
};
