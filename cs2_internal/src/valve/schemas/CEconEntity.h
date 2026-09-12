#pragma once

#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>

class CAttributeList
{
public:
	SCHEMA_FIELD(C_NetworkUtlVectorBase<std::uint8_t>, m_Attributes, "client.dll", "CAttributeList", "m_Attributes")
};

class C_EconItemView
{
public:
	SCHEMA_FIELD(std::uint16_t, m_iItemDefinitionIndex, "client.dll", "C_EconItemView", "m_iItemDefinitionIndex")
	SCHEMA_FIELD(std::uint64_t, m_iItemID, "client.dll", "C_EconItemView", "m_iItemID")
	SCHEMA_FIELD(std::uint32_t, m_iItemIDHigh, "client.dll", "C_EconItemView", "m_iItemIDHigh")
	SCHEMA_FIELD(std::uint32_t, m_iItemIDLow, "client.dll", "C_EconItemView", "m_iItemIDLow")
	SCHEMA_FIELD(std::uint32_t, m_iAccountID, "client.dll", "C_EconItemView", "m_iAccountID")
	SCHEMA_FIELD(std::int32_t, m_iEntityQuality, "client.dll", "C_EconItemView", "m_iEntityQuality")
	SCHEMA_FIELD(bool, m_bInitialized, "client.dll", "C_EconItemView", "m_bInitialized")
	SCHEMA_FIELD(bool, m_bDisallowSOC, "client.dll", "C_EconItemView", "m_bDisallowSOC")
	SCHEMA_FIELD(bool, m_bRestoreCustomMaterialAfterPrecache, "client.dll", "C_EconItemView", "m_bRestoreCustomMaterialAfterPrecache")
	SCHEMA_FIELD(CAttributeList, m_AttributeList, "client.dll", "C_EconItemView", "m_AttributeList")
};

class C_AttributeContainer
{
public:
	SCHEMA_FIELD(C_EconItemView, m_Item, "client.dll", "C_AttributeContainer", "m_Item")
};

class C_EconEntity : public C_BaseEntity
{
public:
	SCHEMA_FIELD(bool, m_bAttributesInitialized, "client.dll", "C_EconEntity", "m_bAttributesInitialized")
	SCHEMA_FIELD(C_AttributeContainer, m_AttributeManager, "client.dll", "C_EconEntity", "m_AttributeManager")
	SCHEMA_FIELD(std::int32_t, m_nFallbackPaintKit, "client.dll", "C_EconEntity", "m_nFallbackPaintKit")
	SCHEMA_FIELD(std::int32_t, m_nFallbackSeed, "client.dll", "C_EconEntity", "m_nFallbackSeed")
	SCHEMA_FIELD(float, m_flFallbackWear, "client.dll", "C_EconEntity", "m_flFallbackWear")
	SCHEMA_FIELD(std::int32_t, m_nFallbackStatTrak, "client.dll", "C_EconEntity", "m_nFallbackStatTrak")
	SCHEMA_FIELD(CHandle<C_BaseEntity>, m_hViewmodelAttachment, "client.dll", "C_EconEntity", "m_hViewmodelAttachment")
};
