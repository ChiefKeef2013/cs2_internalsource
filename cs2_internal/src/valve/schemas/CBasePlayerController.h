#pragma once

#include <valve/classes/CSchemaSystem.h>

class C_BasePlayerPawn;
class C_CSObserverPawn;

class CBasePlayerController
{
public:
	SCHEMA_FIELD(CHandle<C_BasePlayerPawn>, m_hPawn, "client.dll", "CBasePlayerController", "m_hPawn")
	SCHEMA_FIELD(std::uint32_t, m_nTickBase, "client.dll", "CBasePlayerController", "m_nTickBase")
	SCHEMA_FIELD(std::uint64_t, m_steamID, "client.dll", "CBasePlayerController", "m_steamID")
};

class CCSPlayerController_InGameMoneyServices
{
public:
	SCHEMA_FIELD(std::int32_t, m_iAccount, "client.dll", "CCSPlayerController_InGameMoneyServices", "m_iAccount")
};

class CCSPlayerController : public CBasePlayerController
{
public:
	SCHEMA_FIELD(bool, m_bPawnIsAlive, "client.dll", "CCSPlayerController", "m_bPawnIsAlive")
	SCHEMA_FIELD(CHandle<C_CSObserverPawn>, m_hObserverPawn, "client.dll", "CCSPlayerController", "m_hObserverPawn")
	SCHEMA_FIELD(CHandle<C_BasePlayerPawn>, m_hPlayerPawn, "client.dll", "CCSPlayerController", "m_hPlayerPawn")
	SCHEMA_FIELD(std::uint32_t, m_iPing, "client.dll", "CCSPlayerController", "m_iPing")
	SCHEMA_FIELD(std::uintptr_t, m_pInGameMoneyServices, "client.dll", "CCSPlayerController", "m_pInGameMoneyServices")
	SCHEMA_FIELD(CUtlString, m_sSanitizedPlayerName, "client.dll", "CCSPlayerController", "m_sSanitizedPlayerName")
};
