#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace memory {

	namespace detail {

		[[nodiscard]] inline bool is_user_addr( std::uintptr_t address ) noexcept
		{
			return address >= 0x10000ull && address <= 0x00007FFFFFFFFFFFull;
		}

	}

	[[nodiscard]] std::uintptr_t get_module_base( std::string_view module_name );
	[[nodiscard]] std::uintptr_t get_module_export( std::string_view export_name );
	[[nodiscard]] std::uintptr_t get_module_interface( std::string_view interface_name );
	[[nodiscard]] std::uintptr_t get_module_export_with_base (std::uintptr_t module_base, std::string_view export_name);
	[[nodiscard]] std::uintptr_t resolve_pattern( std::string_view pattern );

	[[nodiscard]] std::uintptr_t get_module_export( std::uintptr_t module_base, std::uint16_t ordinal );
	[[nodiscard]] std::uintptr_t get_module_size( std::uintptr_t module_base );

	[[nodiscard]] std::uintptr_t find_vtable_by_rtti( std::uintptr_t module_base, std::string_view class_name );
	[[nodiscard]] std::uintptr_t find_instance_by_rtti( std::uintptr_t module_base, std::string_view class_name );
	[[nodiscard]] std::uintptr_t find_global_instance_by_vtable( std::uintptr_t module_base, std::uintptr_t vtable_address );

	[[nodiscard]] inline bool is_game_ptr( std::uintptr_t address ) noexcept
	{
		return detail::is_user_addr( address ) && ( address & 0x7ull ) == 0;
	}

	template <typename T>
	[[nodiscard]] inline T read( std::uintptr_t address )
	{
		return *reinterpret_cast< T* >( address );
	}

	template <typename T>
	[[nodiscard]] inline std::optional<T> safe_read( std::uintptr_t address )
	{
		if ( !detail::is_user_addr( address ) )
			return std::nullopt;

		return *reinterpret_cast< T* >( address );
	}

	template <typename T>
	inline void write( std::uintptr_t address, const T& value )
	{
		*reinterpret_cast< T* >( address ) = value;
	}

	template <typename T>
	[[nodiscard]] inline bool safe_write( std::uintptr_t address, const T& value )
	{
		if ( !detail::is_user_addr( address ) )
			return false;

		*reinterpret_cast< T* >( address ) = value;
		return true;
	}

	[[nodiscard]] std::uintptr_t get_vfunc( std::uintptr_t instance, std::size_t index );
	[[nodiscard]] std::string read_string( std::uintptr_t address, std::size_t max_length = 256 );

	template <typename T, typename... args_t>
	inline T call( std::uintptr_t address, args_t... args )
	{
		if ( !address )
		{
			if constexpr ( std::is_void_v<T> )
				return;
			else
				return T{};
		}

		return reinterpret_cast< T( __fastcall* )( args_t... ) >( address )( args... );
	}

	template <typename T, typename... args_t>
	inline T call_vfunc( std::uintptr_t instance, std::size_t index, args_t... args )
	{
		const auto fail = []() -> T
		{
			if constexpr ( std::is_void_v<T> )
				return;
			else
				return T{};
		};

		const auto func = get_vfunc( instance, index );
		if ( !func )
			return fail();

		return reinterpret_cast< T( __fastcall* )( std::uintptr_t, args_t... ) >(
			func )( instance, args... );
	}

}
