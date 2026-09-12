#pragma once

#include <cstddef>
#include <cstdint>
#include <valve/utils/math.cpp>

template <typename T = void>
using CHandle = std::uint32_t;

using GameTick_t = std::int32_t;
using GameTime_t = float;
using CUtlStringToken = std::uint32_t;
using MoveType_t = std::uint8_t;
using CSWeaponType = std::int32_t;
using CSWeaponMode = std::int32_t;
using CGlobalSymbol = std::uintptr_t;
using CUtlString = std::uintptr_t;
using CStrongHandle = std::uintptr_t;
using CUtlSymbolLarge = std::uintptr_t;
using CNetworkVelocityVector = math::vector3;
using CNetworkViewOffsetVector = math::vector3;
using CNetworkOriginCellCoordQuantizedVector = math::vector3;

template <typename T>
struct C_NetworkUtlVectorBase
{
	T* memory{};
	std::int32_t allocation_count{};
	std::int32_t grow_size{};
	std::int32_t size{};
};

struct CFiringModeFloat
{
	float values[ 2 ]{};

	[[nodiscard]] float value( std::size_t mode = 0 ) const
	{
		return values[ mode < 2 ? mode : 0 ];
	}
};
