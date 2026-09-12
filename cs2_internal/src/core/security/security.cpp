#pragma once

namespace security {

	namespace integrity {

		struct cached_hash
		{
			std::uintptr_t module_base;
			std::uint32_t crc32;
			bool valid;
		};

		bool initialize( );
		cached_hash* get( std::uintptr_t module_base );

	}

	namespace regions {

		struct region
		{
			std::uintptr_t base;
			std::size_t size;
		};

		void add( std::uintptr_t base, std::size_t size );
		void add_module( HMODULE module );
		bool is_protected( const void* address, std::size_t size );

	}

	namespace prologues {

		struct hook_info
		{
			std::uintptr_t target;
			const std::uint8_t* bytes;
			std::size_t length;
		};

		void add( std::uintptr_t target, const std::uint8_t* bytes, std::size_t length );
		hook_info* get( std::uintptr_t address );

	}

	namespace integrity {

	namespace detail {
		inline std::vector<cached_hash> cached_hashes{};
	}

	inline bool initialize () {

		return true;
	}

	inline cached_hash* get (std::uintptr_t module_base) {
		for (auto& entry : detail::cached_hashes) {
			if (entry.module_base == module_base) {
				return &entry;
			}
		}

		return nullptr;
	}

	}

	namespace prologues {

	namespace detail {

		inline std::vector<hook_info> hooks{};

	}

	inline void add( std::uintptr_t target, const std::uint8_t* bytes, std::size_t length )
	{
		detail::hooks.push_back( { target, bytes, length } );
	}

	inline hook_info* get( std::uintptr_t address )
	{
		for ( auto& hook : detail::hooks )
		{
			if ( address >= hook.target && address < hook.target + hook.length )
			{
				return &hook;
			}
		}

		return nullptr;
	}

	}

	namespace regions {

	namespace detail {

		inline std::vector<region> regions{};

	}

	inline void add( std::uintptr_t base, std::size_t size )
	{
		detail::regions.push_back( { base, size } );
	}

	inline void add_module( HMODULE module )
	{
		const auto base = reinterpret_cast< std::uintptr_t >( module );
		const auto dos = reinterpret_cast< IMAGE_DOS_HEADER* >( base );
		const auto nt = reinterpret_cast< IMAGE_NT_HEADERS64* >( base + dos->e_lfanew );

		add( base, nt->OptionalHeader.SizeOfImage );
	}

	inline bool is_protected( const void* address, std::size_t size )
	{
		const auto addr = reinterpret_cast< std::uintptr_t >( address );
		const auto end = addr + size;

		for ( const auto& region : detail::regions )
		{
			const auto region_end = region.base + region.size;

			if ( addr < region_end && end > region.base )
			{
				return true;
			}
		}

		return false;
	}

	}

}
