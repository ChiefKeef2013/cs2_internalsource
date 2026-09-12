#pragma once

class CCollisionProperty;

class C_BaseEntity
{
public:
	SCHEMA_FIELD(std::uintptr_t, m_pGameSceneNode, "client.dll", "C_BaseEntity", "m_pGameSceneNode")
	SCHEMA_FIELD(std::uintptr_t, m_pCollision, "client.dll", "C_BaseEntity", "m_pCollision")
	SCHEMA_FIELD(std::int32_t, m_iHealth, "client.dll", "C_BaseEntity", "m_iHealth")
	SCHEMA_FIELD(std::uint8_t, m_iTeamNum, "client.dll", "C_BaseEntity", "m_iTeamNum")
	SCHEMA_FIELD(std::uint8_t, m_lifeState, "client.dll", "C_BaseEntity", "m_lifeState")
	SCHEMA_FIELD(std::uint32_t, m_fFlags, "client.dll", "C_BaseEntity", "m_fFlags")
	SCHEMA_FIELD(std::int32_t, m_iEFlags, "client.dll", "C_BaseEntity", "m_iEFlags")
	SCHEMA_FIELD(CHandle<C_BaseEntity>, m_hOwnerEntity, "client.dll", "C_BaseEntity", "m_hOwnerEntity")
	SCHEMA_FIELD(CHandle<C_BaseEntity>, m_hGroundEntity, "client.dll", "C_BaseEntity", "m_hGroundEntity")
	SCHEMA_FIELD(math::vector3, m_vecAbsVelocity, "client.dll", "C_BaseEntity", "m_vecAbsVelocity")
	SCHEMA_FIELD(CNetworkVelocityVector, m_vecVelocity, "client.dll", "C_BaseEntity", "m_vecVelocity")
	SCHEMA_FIELD(math::vector3, m_vecBaseVelocity, "client.dll", "C_BaseEntity", "m_vecBaseVelocity")
	SCHEMA_FIELD(float, m_flSimulationTime, "client.dll", "C_BaseEntity", "m_flSimulationTime")
	SCHEMA_FIELD(std::int32_t, m_nSimulationTick, "client.dll", "C_BaseEntity", "m_nSimulationTick")
	SCHEMA_FIELD(CUtlStringToken, m_nSubclassID, "client.dll", "C_BaseEntity", "m_nSubclassID")
	SCHEMA_FIELD(MoveType_t, m_nActualMoveType, "client.dll", "C_BaseEntity", "m_nActualMoveType")
	SCHEMA_FIELD(float, m_flGravityScale, "client.dll", "C_BaseEntity", "m_flGravityScale")
	SCHEMA_FIELD(float, m_flFriction, "client.dll", "C_BaseEntity", "m_flFriction")
	SCHEMA_FIELD(float, m_flWaterLevel, "client.dll", "C_BaseEntity", "m_flWaterLevel")

	bool is_alive() { return m_lifeState() == 0 && m_iHealth() > 0; }
};

class C_BaseModelEntity : public C_BaseEntity
{
public:
	SCHEMA_FIELD(CNetworkViewOffsetVector, m_vecViewOffset, "client.dll", "C_BaseModelEntity", "m_vecViewOffset")
	SCHEMA_FIELD(CCollisionProperty, m_Collision, "client.dll", "C_BaseModelEntity", "m_Collision")
};
