#pragma once

class CModelState;

class CGameSceneNode
{
public:
	SCHEMA_FIELD(std::uintptr_t, m_pOwner, "client.dll", "CGameSceneNode", "m_pOwner")
	SCHEMA_FIELD(std::uintptr_t, m_pParent, "client.dll", "CGameSceneNode", "m_pParent")
	SCHEMA_FIELD(std::uintptr_t, m_pChild, "client.dll", "CGameSceneNode", "m_pChild")
	SCHEMA_FIELD(std::uintptr_t, m_pNextSibling, "client.dll", "CGameSceneNode", "m_pNextSibling")
	SCHEMA_FIELD(math::vector3, m_vecAbsOrigin, "client.dll", "CGameSceneNode", "m_vecAbsOrigin")
	SCHEMA_FIELD(math::vector3, m_angAbsRotation, "client.dll", "CGameSceneNode", "m_angAbsRotation")
	SCHEMA_FIELD(CNetworkOriginCellCoordQuantizedVector, m_vecOrigin, "client.dll", "CGameSceneNode", "m_vecOrigin")
	SCHEMA_FIELD(math::vector3, m_angRotation, "client.dll", "CGameSceneNode", "m_angRotation")
	SCHEMA_FIELD(bool, m_bDormant, "client.dll", "CGameSceneNode", "m_bDormant")
};

class CSkeletonInstance : public CGameSceneNode
{
public:
	SCHEMA_FIELD(std::uint8_t, m_nHitboxSet, "client.dll", "CSkeletonInstance", "m_nHitboxSet")
	SCHEMA_FIELD(CModelState, m_modelState, "client.dll", "CSkeletonInstance", "m_modelState")
};

class CModelState
{
public:
	SCHEMA_FIELD(CStrongHandle, m_hModel, "client.dll", "CModelState", "m_hModel")
	SCHEMA_FIELD(CUtlSymbolLarge, m_ModelName, "client.dll", "CModelState", "m_ModelName")
	SCHEMA_FIELD(std::uint64_t, m_MeshGroupMask, "client.dll", "CModelState", "m_MeshGroupMask")
};
