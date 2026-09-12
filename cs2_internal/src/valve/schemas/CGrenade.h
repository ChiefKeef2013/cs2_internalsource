#pragma once

class C_CSPlayerPawn;

class C_BaseCSGrenade : public C_BaseEntity
{
public:
	SCHEMA_FIELD(bool, m_bPinPulled, "client.dll", "C_BaseCSGrenade", "m_bPinPulled")
	SCHEMA_FIELD(GameTime_t, m_fThrowTime, "client.dll", "C_BaseCSGrenade", "m_fThrowTime")
	SCHEMA_FIELD(float, m_flThrowStrength, "client.dll", "C_BaseCSGrenade", "m_flThrowStrength")
};

class C_BaseCSGrenadeProjectile : public C_BaseEntity
{
public:
	SCHEMA_FIELD(std::int32_t, m_nExplodeEffectTickBegin, "client.dll", "C_BaseCSGrenadeProjectile", "m_nExplodeEffectTickBegin")
	SCHEMA_FIELD(math::vector3, m_vInitialPosition, "client.dll", "C_BaseCSGrenadeProjectile", "m_vInitialPosition")
	SCHEMA_FIELD(math::vector3, m_vInitialVelocity, "client.dll", "C_BaseCSGrenadeProjectile", "m_vInitialVelocity")
};

class C_BaseGrenade : public C_BaseEntity
{
public:
	SCHEMA_FIELD(CHandle<C_CSPlayerPawn>, m_hThrower, "client.dll", "C_BaseGrenade", "m_hThrower")
	SCHEMA_FIELD(float, m_DmgRadius, "client.dll", "C_BaseGrenade", "m_DmgRadius")
	SCHEMA_FIELD(float, m_flDamage, "client.dll", "C_BaseGrenade", "m_flDamage")
	SCHEMA_FIELD(float, m_flDetonateTime, "client.dll", "C_BaseGrenade", "m_flDetonateTime")
};

class C_DecoyProjectile : public C_BaseCSGrenadeProjectile
{
public:
	SCHEMA_FIELD(std::int32_t, m_nDecoyShotTick, "client.dll", "C_DecoyProjectile", "m_nDecoyShotTick")
};

class C_Inferno : public C_BaseEntity
{
public:
	SCHEMA_ARRAY(bool, m_bFireIsBurning, 64, "client.dll", "C_Inferno", "m_bFireIsBurning")
	SCHEMA_FIELD(std::int32_t, m_fireCount, "client.dll", "C_Inferno", "m_fireCount")
	SCHEMA_FIELD(float, m_nFireLifetime, "client.dll", "C_Inferno", "m_nFireLifetime")
	SCHEMA_ARRAY(math::vector3, m_firePositions, 64, "client.dll", "C_Inferno", "m_firePositions")
};

class C_SmokeGrenadeProjectile : public C_BaseCSGrenadeProjectile
{
public:
	SCHEMA_FIELD(bool, m_bDidSmokeEffect, "client.dll", "C_SmokeGrenadeProjectile", "m_bDidSmokeEffect")
	SCHEMA_FIELD(std::int32_t, m_nSmokeEffectTickBegin, "client.dll", "C_SmokeGrenadeProjectile", "m_nSmokeEffectTickBegin")
};
