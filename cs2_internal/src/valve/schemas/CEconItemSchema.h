#pragma once

#include <cstdint>

namespace econ_schema {

	namespace item_system
	{
		constexpr std::uintptr_t schema_ptr = 0x8;
	}

	namespace item_schema
	{
		constexpr std::uintptr_t item_count = 0x128;
		constexpr std::uintptr_t item_array = 0x130;
		constexpr std::uintptr_t paint_count = 0x2F0;
		constexpr std::uintptr_t paint_nodes = 0x2F8;
	}

	namespace entity
	{
		constexpr std::uintptr_t composite_material = 0x608;
	}

	namespace paint_kit
	{
		constexpr std::uintptr_t id = 0x00;
		constexpr std::uintptr_t name = 0x08;
		constexpr std::uintptr_t desc_token = 0x10;
		constexpr std::uintptr_t name_token = 0x18;
		constexpr std::uintptr_t rarity = 0x44;
		constexpr std::uintptr_t style = 0x48;
		constexpr std::uintptr_t colors = 0x4C;
		constexpr std::uintptr_t wear_min = 0x6C;
		constexpr std::uintptr_t wear_max = 0x70;
		constexpr std::uintptr_t use_legacy_model = 0xB2;
	}

	namespace attribute
	{
		constexpr std::ptrdiff_t stride = 0x48;
		constexpr std::ptrdiff_t definition = 0x30;
		constexpr std::ptrdiff_t value = 0x34;
		constexpr std::ptrdiff_t vector_size = 0x10;
	}

}
