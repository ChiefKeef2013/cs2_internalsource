#pragma once

class C_PlantedC4 : public C_BaseEntity
{
public:
	SCHEMA_FIELD(bool, m_bBeingDefused, "client.dll", "C_PlantedC4", "m_bBeingDefused")
	SCHEMA_FIELD(bool, m_bBombDefused, "client.dll", "C_PlantedC4", "m_bBombDefused")
	SCHEMA_FIELD(bool, m_bHasExploded, "client.dll", "C_PlantedC4", "m_bHasExploded")
	SCHEMA_FIELD(GameTime_t, m_flC4Blow, "client.dll", "C_PlantedC4", "m_flC4Blow")
	SCHEMA_FIELD(float, m_flTimerLength, "client.dll", "C_PlantedC4", "m_flTimerLength")
	SCHEMA_FIELD(std::int32_t, m_nBombSite, "client.dll", "C_PlantedC4", "m_nBombSite")
};
