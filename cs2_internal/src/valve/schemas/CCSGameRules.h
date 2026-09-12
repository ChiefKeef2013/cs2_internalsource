#pragma once

class C_CSGameRules
{
public:
	SCHEMA_FIELD(bool, m_bFreezePeriod, "client.dll", "C_CSGameRules", "m_bFreezePeriod")
	SCHEMA_FIELD(bool, m_bTeamIntroPeriod, "client.dll", "C_CSGameRules", "m_bTeamIntroPeriod")
	SCHEMA_FIELD(GameTime_t, m_fRoundStartTime, "client.dll", "C_CSGameRules", "m_fRoundStartTime")
	SCHEMA_FIELD(std::int32_t, m_gamePhase, "client.dll", "C_CSGameRules", "m_gamePhase")
};
