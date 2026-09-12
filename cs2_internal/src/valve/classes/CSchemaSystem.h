#pragma once

#include <cstdint>
#include <cstring>
#include <valve/classes/schema_types.h>
#include <valve/utils/fnv1a.cpp>

namespace systems {

	class schemas
	{
	public:
		[[nodiscard]] static std::uint32_t lookup( const char* class_name, std::uint32_t field_hash );
	};

}

#define SCHEMA_FIELD(type, name, mod, netvarClass, field) \
	type& name() \
	{ \
		static const auto _off = systems::schemas::lookup( netvarClass, fnv1a::hash( field, sizeof( field ) - 1 ) ); \
		return *reinterpret_cast<type*>( reinterpret_cast<std::uintptr_t>( this ) + _off ); \
	}

#define SCHEMA_ARRAY(type, name, size, mod, netvarClass, field) \
	type* name##_array() \
	{ \
		static const auto _off = systems::schemas::lookup( netvarClass, fnv1a::hash( field, sizeof( field ) - 1 ) ); \
		return reinterpret_cast<type*>( reinterpret_cast<std::uintptr_t>( this ) + _off ); \
	} \
	type& name##_at( int index ) \
	{ \
		return name##_array()[ index ]; \
	}
