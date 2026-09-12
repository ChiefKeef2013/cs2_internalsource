#pragma once

#include <core/memory.hpp>

namespace features::esp::detail {

	inline constexpr std::size_t primitive_size{ 0x70 };
	inline constexpr std::size_t primitive_scene_object_offset{ 0x18 };
	inline constexpr std::size_t primitive_material_offset{ 0x20 };
	inline constexpr std::size_t primitive_material_copy_offset{ 0x28 };
	inline constexpr std::size_t primitive_color_offset{ 0x50 };
	inline constexpr std::size_t primitive_opacity_offset{ 0x54 };
	inline constexpr std::size_t primitive_sort_key_offset{ 0x5C };
	inline constexpr std::size_t primitive_material_flag_offset{ 0x61 };
	inline constexpr std::size_t primitive_flags_offset{ 0x62 };
	inline constexpr std::uint16_t primitive_draw_last{ 0x8 };

	struct mesh_primitive
	{
		std::uintptr_t unk_00{};
		std::uintptr_t mesh{};
		std::byte pad_10[ 0x8 ]{};
		std::uintptr_t scene_object{};
		std::uintptr_t material{};
		std::uintptr_t material_copy{};
		std::uintptr_t unk_30{};
		std::uintptr_t unk_38{};
		std::uintptr_t unk_40{};
		std::uintptr_t unk_48{};
		std::uint32_t color{};
		float opacity{};
		std::uint32_t unk_58{};
		std::uint16_t sort_key{};
		std::uint16_t unk_5e{};
		std::uint8_t pad_60{};
		std::uint8_t material_flag{};
		std::uint16_t flags{};
		std::uint16_t flags_2{};
		std::byte pad_66[ 0xA ]{};
	};

	static_assert( sizeof( mesh_primitive ) == primitive_size );
	static_assert( offsetof( mesh_primitive, scene_object ) == primitive_scene_object_offset );
	static_assert( offsetof( mesh_primitive, material ) == primitive_material_offset );
	static_assert( offsetof( mesh_primitive, material_copy ) == primitive_material_copy_offset );
	static_assert( offsetof( mesh_primitive, color ) == primitive_color_offset );
	static_assert( offsetof( mesh_primitive, opacity ) == primitive_opacity_offset );
	static_assert( offsetof( mesh_primitive, sort_key ) == primitive_sort_key_offset );
	static_assert( offsetof( mesh_primitive, material_flag ) == primitive_material_flag_offset );
	static_assert( offsetof( mesh_primitive, flags ) == primitive_flags_offset );

	struct primitive_output_buffer
	{
		std::uintptr_t fixed_data{};
		std::int32_t fixed_capacity{};
		std::int32_t fixed_count{};
		std::int32_t overflow_count{};
		std::uint32_t reserved_14{};
		std::uintptr_t overflow_data{};
		std::int32_t overflow_capacity{};
		std::uint32_t overflow_allocation_flags{};

		[[nodiscard]] int count() const noexcept
		{
			if ( fixed_count < 0 || overflow_count < 0 ||
				fixed_capacity < 0 || overflow_capacity < 0 ||
				fixed_count > fixed_capacity ||
				overflow_count > overflow_capacity ||
				( fixed_count && !fixed_data ) ||
				( overflow_count && !overflow_data ) ) {
				return -1;
			}

			constexpr auto sane_primitive_limit{ 1 << 20 };
			if ( fixed_count > sane_primitive_limit - overflow_count ) {
				return -1;
			}

			return fixed_count + overflow_count;
		}

		[[nodiscard]] std::uintptr_t at( int index ) const noexcept
		{
			const auto total = this->count();
			if ( index < 0 || total < 0 || index >= total ) {
				return 0;
			}

			if ( index < fixed_count ) {
				return fixed_data + static_cast<std::size_t>( index ) * primitive_size;
			}

			return overflow_data +
				static_cast<std::size_t>( index - fixed_count ) * primitive_size;
		}
	};

	static_assert( sizeof( primitive_output_buffer ) == 0x28 );
	static_assert( offsetof( primitive_output_buffer, fixed_count ) == 0xC );
	static_assert( offsetof( primitive_output_buffer, overflow_count ) == 0x10 );
	static_assert( offsetof( primitive_output_buffer, overflow_data ) == 0x18 );

	inline constexpr std::uint32_t k_managed_scene_owner{ 0x7FFEu };

	[[nodiscard]] inline bool is_chams_schema( const char* name ) noexcept
	{
		if ( !name || name[ 0 ] != 'C' || name[ 1 ] != '_' )
		{
			return false;
		}

		const auto third = name[ 2 ];
		if ( third == 'C' || third == 'W' || third == 'K' )
		{
			return true;
		}

		return ( third == 'A' && name[ 3 ] == 'K' ) || ( third == 'D' && name[ 3 ] == 'E' );
	}

	[[nodiscard]] inline bool scene_object_freed( std::uintptr_t scene_object ) noexcept
	{
		constexpr std::uint64_t freed_flag{ 0x1000000000000000ull };
		if ( !memory::is_game_ptr( scene_object ) ) {
			return true;
		}

		const auto flags = memory::safe_read<std::uint64_t>( scene_object + 128 );
		return !flags || ( *flags & freed_flag ) != 0;
	}

	[[nodiscard]] inline bool scene_object_live( std::uintptr_t scene_object ) noexcept
	{
		if ( scene_object_freed( scene_object ) ) {
			return false;
		}

		const auto deleted = memory::safe_read<std::uint8_t>( scene_object + 154 );
		if ( !deleted || ( *deleted & 0x20 ) != 0 ) {
			return false;
		}

		return true;
	}

	[[nodiscard]] inline bool is_managed_scene_object( std::uintptr_t scene_object ) noexcept
	{
		if ( !memory::is_game_ptr( scene_object ) ) {
			return false;
		}

		const auto owner = memory::safe_read<std::uint32_t>( scene_object + 0xC0 );
		return owner && *owner == k_managed_scene_owner;
	}

	[[nodiscard]] inline std::optional<primitive_output_buffer> read_primitive_buffer(
		std::uintptr_t address )
	{
		if ( !address ) {
			return std::nullopt;
		}

		const auto result = memory::read<primitive_output_buffer>( address );
		if ( result.count() < 0 ) {
			return std::nullopt;
		}

		return result;
	}

	inline void replace_primitive(
		std::uintptr_t primitive, std::uintptr_t material,
		const xdraw::color& color )
	{
		if ( !primitive || !material || !memory::is_game_ptr( material ) ) {
			return;
		}

		const auto vtable = memory::safe_read<std::uintptr_t>( material );
		if ( !vtable || !memory::is_game_ptr( *vtable ) ) {
			return;
		}

		memory::write<std::uintptr_t>(
			primitive + primitive_material_offset, material );
		memory::write<std::uintptr_t>(
			primitive + primitive_material_copy_offset, material );
		memory::write<std::uint32_t>(
			primitive + primitive_color_offset, color );
	}

	inline void mark_primitive_last( std::uintptr_t primitive )
	{
		if ( !primitive ) {
			return;
		}

		const auto flags = memory::read<std::uint16_t>(
			primitive + primitive_flags_offset );
		memory::write<std::uint16_t>(
			primitive + primitive_flags_offset,
			static_cast<std::uint16_t>( flags | primitive_draw_last ) );

		const auto order = memory::read<std::uint16_t>(
			primitive + primitive_sort_key_offset );
		if ( order < 0xFFFFu ) {
			memory::write<std::uint16_t>(
				primitive + primitive_sort_key_offset, static_cast<std::uint16_t>( order + 1 ) );
		}
	}

}
