#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace valve {

	struct trace_filter
	{
		std::uintptr_t vtable;
		std::uintptr_t mask;
		std::array<std::int64_t, 2> v1;
		std::array<int, 4> skip_handles;
		std::array<std::int16_t, 2> collisions;
		std::int16_t v2;
		std::uint8_t layer;
		std::uint8_t flags;
		std::uint8_t v5;
		std::uint8_t v6;
		std::byte pad0[ 0x6 ];
		char v7;
	};

	struct trace_ray
	{
		math::vector3 mins;
		math::vector3 maxs;
		std::byte pad0[ 0x10 ];
		std::uint8_t type;
		std::byte pad1[ 0x7 ];
	};

	static_assert( offsetof( trace_ray, type ) == 40 );

	struct trace_result
	{
		void* surface;
		std::uintptr_t hit_entity;
		void* hitbox_data;
		std::byte pad0[ 0x38 ];
		std::uint32_t contents;
		std::byte pad1[ 0x24 ];
		math::vector3 start_pos;
		math::vector3 end_pos;
		math::vector3 normal;
		math::vector3 position;
		std::byte pad2[ 0x4 ];
		float fraction;
		std::uint64_t unk_b0;
		std::uint16_t unk_b8;
		std::uint8_t ray_type;
		bool all_solid;
		std::byte pad4[ 0x14 ];
	};

	static_assert( offsetof( trace_result, hit_entity ) == 8 );
	static_assert( offsetof( trace_result, start_pos ) == 120 );
	static_assert( offsetof( trace_result, end_pos ) == 132 );
	static_assert( offsetof( trace_result, fraction ) == 172 );
	static_assert( offsetof( trace_result, all_solid ) == 187 );

	struct trace_array_element
	{
		std::byte pad0[ 0x38 ];
	};

	struct trace_data
	{
		int unknown1;
		float unknown2{ 52.0f };
		void* array_pointer;
		int unknown3{ 128 };
		int unknown4{ static_cast< int >( 0x80000000 ) };
		std::array<trace_array_element, 0x80> elements;
		std::byte pad0[ 0x8 ];
		int num_hits;
		int unknown5;
		void* hit_array_pointer;
		int hit_capacity{ 8 };
		int hit_flags{ static_cast< int >( 0x80000000 ) };
		std::byte hit_elements[ 0xc0 ];
		math::vector3 start;
		math::vector3 direction;
		float fraction{ 1.0f };
		bool unknown6;
		std::byte pad1[ 0x4b ];
	};

	static_assert( offsetof( trace_data, elements ) == 0x18 );
	static_assert( offsetof( trace_data, num_hits ) == 0x1c20 );
	static_assert( offsetof( trace_data, hit_array_pointer ) == 0x1c28 );
	static_assert( offsetof( trace_data, start ) == 0x1cf8 );
	static_assert( sizeof( trace_data ) == 0x1d60 );

	struct player_movement_filter
	{
		std::byte data[ 0x48 ];
	};

	struct bbox_collision
	{
		math::vector3 mins;
		math::vector3 maxs;
	};

}
