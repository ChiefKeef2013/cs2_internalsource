#pragma once

#include <cstring>
#include <string>
#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerPawn.h>
#include <valve/schemas/CEconEntity.h>
#include <valve/schemas/CGameSceneNode.h>
#include <valve/schemas/CWeapon.h>
#include <valve/schemas/CEconItemSchema.h>
#include <safetyhook/safetyhook.hpp>

namespace features::changer::custom_paint {

namespace detail {

	constexpr int snap_slots = 64;
	constexpr int k_max_loose_vars = 128;

#pragma pack( push, 1 )
	struct composite_loose_var
	{
		char* name;
		std::uint8_t pad_008[ 0x38 ];
		std::int32_t type;
		std::uint8_t pad_044[ 0x18 ];
		float float_x;
		float float_x_min;
		float float_x_max;
		float float_y;
		float float_y_min;
		float float_y_max;
		float float_z;
		float float_z_min;
		float float_z_max;
		float float_w;
		float float_w_min;
		float float_w_max;
		std::uint32_t color4;
		std::uint8_t pad_090[ 0x288 - 0x90 ];
	};
#pragma pack( pop )
	static_assert( sizeof( composite_loose_var ) == 0x288 );
	static_assert( offsetof( composite_loose_var, type ) == 0x40 );
	static_assert( offsetof( composite_loose_var, color4 ) == 0x8C );

	struct loose_var_vector
	{
		int size;
		int pad4;
		std::uint8_t* memory;
		int capacity;
		int grow_flags;
	};

	struct kit_color_snap
	{
		int id{};
		int style{};
		std::uint8_t raw[ 16 ]{};
		bool filler{ true };
		bool ok{};
	};

	inline safetyhook::MidHook g_mid_weapon{};
	inline safetyhook::MidHook g_mid_glove{};
	inline bool g_ready{};
	inline kit_color_snap g_snaps[ snap_slots ]{};
	inline std::atomic<int> g_rebuild_depth{};
	inline std::atomic<bool> g_level_busy{ true };
	inline std::atomic<bool> g_map_playable{ false };
	inline std::atomic<std::int64_t> g_spawned_at_ms{};

	[[nodiscard]] inline std::uint8_t clamp_u8( float v )
	{
		if ( v < 0.f ) v = 0.f;
		if ( v > 1.f ) v = 1.f;
		return static_cast< std::uint8_t >( v * 255.f + 0.5f );
	}

	[[nodiscard]] inline std::uint32_t pack_color( float r, float g, float b )
	{
		return static_cast< std::uint32_t >( clamp_u8( r ) )
			| ( static_cast< std::uint32_t >( clamp_u8( g ) ) << 8 )
			| ( static_cast< std::uint32_t >( clamp_u8( b ) ) << 16 )
			| 0xFF000000u;
	}

	[[nodiscard]] inline std::uint32_t pack_color_arr( const float* rgba )
	{
		return pack_color( rgba[ 0 ], rgba[ 1 ], rgba[ 2 ] );
	}

	inline void fill_color_fields( composite_loose_var& v, std::uint32_t rgba )
	{
		v.color4 = rgba;

		const float r = ( rgba & 0xFF ) / 255.f;
		const float g = ( ( rgba >> 8 ) & 0xFF ) / 255.f;
		const float b = ( ( rgba >> 16 ) & 0xFF ) / 255.f;
		const float a = ( ( rgba >> 24 ) & 0xFF ) / 255.f;

		v.float_x = r; v.float_y = g; v.float_z = b; v.float_w = a;
		v.float_x_min = 0.f; v.float_x_max = 1.f;
		v.float_y_min = 0.f; v.float_y_max = 1.f;
		v.float_z_min = 0.f; v.float_z_max = 1.f;
		v.float_w_min = 0.f; v.float_w_max = 1.f;
	}

	inline void fill_float3_fields( composite_loose_var& v, float r, float g, float b )
	{
		v.float_x = r; v.float_y = g; v.float_z = b; v.float_w = 1.f;
		v.float_x_min = 0.f; v.float_x_max = 1.f;
		v.float_y_min = 0.f; v.float_y_max = 1.f;
		v.float_z_min = 0.f; v.float_z_max = 1.f;
		v.float_w_min = 0.f; v.float_w_max = 1.f;
		v.color4 = pack_color( r, g, b );
	}

	[[nodiscard]] inline bool usable_addr( std::uintptr_t address )
	{
		if ( address < 0x10000ull )
			return false;
		if ( ( address >> 48 ) != 0 )
			return false;
		if ( ( address & 7ull ) != 0 )
			return false;
		if ( ( address & 0xffffffffull ) == 0 )
			return false;
		if ( ( address >> 32 ) == 0 )
			return false;
		return true;
	}

	[[nodiscard]] inline loose_var_vector* as_loose_vector( void* vector )
	{
		if ( !vector )
			return nullptr;

		const auto addr = reinterpret_cast< std::uintptr_t >( vector );
		if ( !usable_addr( addr ) )
			return nullptr;

		auto* vec = reinterpret_cast< loose_var_vector* >( vector );
		if ( vec->size <= 0 || vec->size > k_max_loose_vars )
			return nullptr;
		if ( vec->capacity < vec->size || vec->capacity > k_max_loose_vars * 2 )
			return nullptr;
		if ( !vec->memory )
			return nullptr;

		const auto mem = reinterpret_cast< std::uintptr_t >( vec->memory );
		if ( !usable_addr( mem ) )
			return nullptr;

		return vec;
	}

	[[nodiscard]] inline const char* loose_name( const composite_loose_var& elem )
	{
		if ( !elem.name )
			return nullptr;

		const auto addr = reinterpret_cast< std::uintptr_t >( elem.name );
		if ( !usable_addr( addr ) )
			return nullptr;

		return elem.name;
	}

	inline bool set_named_color( void* vector, const char* name, std::uint32_t rgba )
	{
		auto* vec = as_loose_vector( vector );
		if ( !vec || !name )
			return false;

		for ( int i = 0; i < vec->size; ++i )
		{
			auto* elem = reinterpret_cast< composite_loose_var* >( vec->memory + static_cast< std::size_t >( i ) * 0x288 );
			const auto* existing = loose_name( *elem );
			if ( !existing || std::strcmp( existing, name ) != 0 )
				continue;

			fill_color_fields( *elem, rgba );
			return true;
		}

		return false;
	}

	inline bool set_named_float3( void* vector, const char* name, float r, float g, float b )
	{
		auto* vec = as_loose_vector( vector );
		if ( !vec || !name )
			return false;

		for ( int i = 0; i < vec->size; ++i )
		{
			auto* elem = reinterpret_cast< composite_loose_var* >( vec->memory + static_cast< std::size_t >( i ) * 0x288 );
			const auto* existing = loose_name( *elem );
			if ( !existing || std::strcmp( existing, name ) != 0 )
				continue;

			fill_float3_fields( *elem, r, g, b );
			return true;
		}

		return false;
	}

	inline void overwrite_gv_colors( void* vector, const std::uint32_t colors[ 4 ] )
	{
		auto* vec = as_loose_vector( vector );
		if ( !vec || !colors )
			return;

		for ( int i = 0; i < vec->size; ++i )
		{
			auto* elem = reinterpret_cast< composite_loose_var* >( vec->memory + static_cast< std::size_t >( i ) * 0x288 );
			const auto* existing = loose_name( *elem );
			if ( !existing )
				continue;
			if ( std::strncmp( existing, "g_vColor", 8 ) != 0 )
				continue;
			if ( existing[ 8 ] < '0' || existing[ 8 ] > '3' || existing[ 9 ] != '\0' )
				continue;

			fill_color_fields( *elem, colors[ existing[ 8 ] - '0' ] );
		}
	}

	inline void lift_inject( std::uint32_t colors[ 4 ], bool aggressive )
	{
		const float floor_v = aggressive ? 0.72f : 0.55f;
		for ( int i = 0; i < 4; ++i )
		{
			float r = ( colors[ i ] & 0xFF ) / 255.f;
			float g = ( ( colors[ i ] >> 8 ) & 0xFF ) / 255.f;
			float b = ( ( colors[ i ] >> 16 ) & 0xFF ) / 255.f;

			const float mx = ( std::max )( r, ( std::max )( g, b ) );
			if ( mx < 1e-4f )
			{
				r = g = b = floor_v;
			}
			else if ( mx < floor_v )
			{
				const float s = floor_v / mx;
				r = ( std::min )( 1.f, r * s );
				g = ( std::min )( 1.f, g * s );
				b = ( std::min )( 1.f, b * s );
			}

			colors[ i ] = pack_color( r, g, b );
		}
	}

	inline void append_color_vars( void* vector, const std::uint32_t colors[ 4 ], bool is_glove )
	{
		if ( !vector || !colors )
			return;

		overwrite_gv_colors( vector, colors );

		for ( int i = 0; i < 4; ++i )
		{
			char name[ 16 ];
			std::snprintf( name, sizeof( name ), "g_vColor%d", i );
			set_named_color( vector, name, colors[ i ] );
		}

		set_named_color( vector, "g_vColor", colors[ 0 ] );

		if ( is_glove )
		{
			for ( int i = 0; i < 4; ++i )
			{
				char top[ 32 ], bot[ 32 ];
				std::snprintf( top, sizeof( top ), "g_vGloveColorTop%d", i );
				std::snprintf( bot, sizeof( bot ), "g_vGloveColorBottom%d", i );
				set_named_color( vector, top, colors[ i ] );
				set_named_color( vector, bot, colors[ i ] );
			}

			const float r0 = ( colors[ 0 ] & 0xFF ) / 255.f;
			const float g0 = ( ( colors[ 0 ] >> 8 ) & 0xFF ) / 255.f;
			const float b0 = ( ( colors[ 0 ] >> 16 ) & 0xFF ) / 255.f;
			const float r1 = ( colors[ 1 ] & 0xFF ) / 255.f;
			const float g1 = ( ( colors[ 1 ] >> 8 ) & 0xFF ) / 255.f;
			const float b1 = ( ( colors[ 1 ] >> 16 ) & 0xFF ) / 255.f;

			set_named_float3( vector, "g_vColorTint", r0, g0, b0 );
			set_named_float3( vector, "g_vColor0", r0, g0, b0 );
			set_named_float3( vector, "g_vColor1", r1, g1, b1 );
		}
	}

	[[nodiscard]] inline kit_color_snap* find_snap( int paint_kit_id )
	{
		for ( int i = 0; i < snap_slots; ++i )
		{
			if ( g_snaps[ i ].ok && g_snaps[ i ].id == paint_kit_id )
				return &g_snaps[ i ];
		}

		return nullptr;
	}

	[[nodiscard]] inline std::uintptr_t get_schema( )
	{
		const auto slot = addresses::globals::item_system_instance;
		if ( !slot )
			return 0;

		const auto system = memory::read<std::uintptr_t>( slot );
		if ( !system || !memory::is_game_ptr( system ) )
			return 0;

		const auto schema = memory::read<std::uintptr_t>( system + econ_schema::item_system::schema_ptr );
		if ( !schema || !memory::is_game_ptr( schema ) )
			return 0;

		return schema;
	}

	[[nodiscard]] inline void* find_paint_kit_ptr( int paint_kit_id )
	{
		if ( paint_kit_id <= 0 )
			return nullptr;

		const auto schema = get_schema( );
		if ( !schema )
			return nullptr;

		struct node { int left, right, pad8, pad12, key, pad20; void* value; };
		const auto count = memory::read<int>( schema + econ_schema::item_schema::paint_count );
		auto* nodes = memory::read<node*>( schema + econ_schema::item_schema::paint_nodes );

		if ( count <= 0 || count > 20000 || !nodes || !memory::is_game_ptr( reinterpret_cast< std::uintptr_t >( nodes ) ) )
			return nullptr;

		for ( int i = 0; i < count; ++i )
		{
			if ( nodes[ i ].key != paint_kit_id || !nodes[ i ].value )
				continue;
			if ( memory::is_game_ptr( reinterpret_cast< std::uintptr_t >( nodes[ i ].value ) ) )
				return nodes[ i ].value;
		}

		return nullptr;
	}

	[[nodiscard]] inline bool vector_has_gv_color( void* vector )
	{
		auto* vec = as_loose_vector( vector );
		if ( !vec )
			return false;

		for ( int i = 0; i < vec->size; ++i )
		{
			auto* elem = reinterpret_cast< composite_loose_var* >( vec->memory + static_cast< std::size_t >( i ) * 0x288 );
			const auto* existing = loose_name( *elem );
			if ( !existing )
				continue;
			if ( std::strncmp( existing, "g_vColor", 8 ) == 0 )
				return true;
			if ( std::strncmp( existing, "g_vGloveColor", 13 ) == 0 )
				return true;
		}

		return false;
	}

	[[nodiscard]] inline kit_color_snap* ensure_snap( int paint_kit_id )
	{
		if ( auto* s = find_snap( paint_kit_id ) )
			return s;

		auto* kit = find_paint_kit_ptr( paint_kit_id );
		if ( !kit )
			return nullptr;

		kit_color_snap tmp{};
		tmp.id = paint_kit_id;
		tmp.style = *reinterpret_cast< int* >( reinterpret_cast< std::uint8_t* >( kit ) + econ_schema::paint_kit::style );
		std::memcpy( tmp.raw, reinterpret_cast< std::uint8_t* >( kit ) + econ_schema::paint_kit::colors, 16 );

		bool all_filler = true;
		for ( int i = 0; i < 16; ++i )
		{
			if ( tmp.raw[ i ] != 0x80 )
			{
				all_filler = false;
				break;
			}
		}
		tmp.filler = all_filler;

		const bool pattern_style = tmp.style == 2 || tmp.style == 3 || tmp.style == 5 || tmp.style == 6 || tmp.style == 7;
		if ( pattern_style )
			tmp.filler = true;

		tmp.ok = true;

		for ( int i = 0; i < snap_slots; ++i )
		{
			if ( !g_snaps[ i ].ok )
			{
				g_snaps[ i ] = tmp;
				return &g_snaps[ i ];
			}
		}

		static int rr = 0;
		g_snaps[ rr++ % snap_slots ] = tmp;
		return &g_snaps[ ( rr - 1 ) % snap_slots ];
	}

	[[nodiscard]] inline bool style_uses_kit_colors( int style )
	{
		return style == 0 || style == 1 || style == 4 || style == 8 || style == 9;
	}

	inline void write_kit_colors( int paint_kit_id, const float* rgba16, bool allow_filler )
	{
		if ( paint_kit_id <= 0 || !rgba16 )
			return;

		auto* snap = find_snap( paint_kit_id );
		if ( !snap )
			snap = ensure_snap( paint_kit_id );
		if ( !snap )
			return;

		if ( !style_uses_kit_colors( snap->style ) && !allow_filler )
			return;
		if ( snap->filler && !allow_filler )
			return;

		auto* kit = find_paint_kit_ptr( paint_kit_id );
		if ( !kit )
			return;

		auto* base = reinterpret_cast< std::uint8_t* >( kit ) + econ_schema::paint_kit::colors;
		for ( int i = 0; i < 4; ++i )
		{
			base[ i * 4 + 0 ] = clamp_u8( rgba16[ i * 4 + 0 ] );
			base[ i * 4 + 1 ] = clamp_u8( rgba16[ i * 4 + 1 ] );
			base[ i * 4 + 2 ] = clamp_u8( rgba16[ i * 4 + 2 ] );
			base[ i * 4 + 3 ] = 0xFF;
		}
	}

	inline void restore_kit_colors_impl( int paint_kit_id )
	{
		if ( paint_kit_id <= 0 )
			return;

		auto* snap = find_snap( paint_kit_id );
		if ( !snap )
			return;

		auto* kit = find_paint_kit_ptr( paint_kit_id );
		if ( !kit )
			return;

		*reinterpret_cast< int* >( reinterpret_cast< std::uint8_t* >( kit ) + econ_schema::paint_kit::style ) = snap->style;
		if ( snap->filler )
			std::memset( reinterpret_cast< std::uint8_t* >( kit ) + econ_schema::paint_kit::colors, 0x80, 16 );
		else
			std::memcpy( reinterpret_cast< std::uint8_t* >( kit ) + econ_schema::paint_kit::colors, snap->raw, 16 );
	}

	[[nodiscard]] inline std::uint16_t def_index_of( void* entity )
	{
		if ( !entity )
			return 0;

		const auto base = reinterpret_cast< std::uintptr_t >( entity );
		if ( !memory::is_game_ptr( base ) )
			return 0;

		auto& item = reinterpret_cast< C_EconEntity* >( entity )->m_AttributeManager( ).m_Item( );
		return item.m_iItemDefinitionIndex( );
	}

	[[nodiscard]] inline bool is_glove_def( std::uint16_t d )
	{
		return ( d >= 4700 && d <= 5100 );
	}

	[[nodiscard]] inline bool any_custom_color_active( )
	{
		for ( const auto& [def, skin] : settings::g_changer.skins.data )
		{
			if ( skin.use_custom_colors )
				return true;
		}
		return false;
	}

	inline bool resolve_colors( void* entity, int path_hint, std::uint32_t out[ 4 ], std::uint16_t* out_def )
	{
		const auto def = def_index_of( entity );
		if ( out_def )
			*out_def = def;

		std::int16_t lookup_def = static_cast< std::int16_t >( def );

		if ( path_hint == 2 )
		{
			const bool entity_is_glove = is_glove_def( static_cast< std::uint16_t >( def ) );
			const auto direct_it = settings::g_changer.skins.data.find( lookup_def );
			const bool direct_ok = entity_is_glove
				&& direct_it != settings::g_changer.skins.data.end( )
				&& direct_it->second.use_custom_colors;

			if ( !direct_ok )
			{
				lookup_def = 0;
				for ( const auto& [d, skin] : settings::g_changer.skins.data )
				{
					if ( is_glove_def( static_cast< std::uint16_t >( d ) ) && skin.use_custom_colors )
					{
						lookup_def = d;
						break;
					}
				}
			}
		}

		if ( lookup_def == 0 )
			return false;

		if ( out_def )
			*out_def = static_cast< std::uint16_t >( lookup_def );

		const auto it = settings::g_changer.skins.data.find( lookup_def );
		if ( it == settings::g_changer.skins.data.end( ) )
			return false;

		const auto& skin = it->second;
		if ( !skin.use_custom_colors )
			return false;

		out[ 0 ] = pack_color_arr( skin.custom_colors + 0 );
		out[ 1 ] = pack_color_arr( skin.custom_colors + 4 );
		out[ 2 ] = pack_color_arr( skin.custom_colors + 8 );
		out[ 3 ] = pack_color_arr( skin.custom_colors + 12 );

		return true;
	}

	inline void on_build_material( safetyhook::Context& ctx, int path_hint )
	{
		if ( g_level_busy.load( std::memory_order_acquire ) || !g_map_playable.load( std::memory_order_acquire ) )
			return;
		if ( g_rebuild_depth.load( std::memory_order_acquire ) <= 0 )
			return;

		if ( !any_custom_color_active( ) )
			return;

		auto* vector = reinterpret_cast< void* >( ctx.r8 );
		if ( !vector_has_gv_color( vector ) )
			return;

		auto* entity = ( path_hint == 2 )
			? reinterpret_cast< void* >( ctx.rdi )
			: reinterpret_cast< void* >( ctx.rbx );

		std::uint32_t colors[ 4 ]{};
		std::uint16_t def{};

		if ( !resolve_colors( entity, path_hint, colors, &def ) )
		{
			if ( path_hint == 2 && ctx.rbx )
			{
				if ( !resolve_colors( reinterpret_cast< void* >( ctx.rbx ), path_hint, colors, &def ) )
					return;
			}
			else
			{
				return;
			}
		}

		const bool is_glove = is_glove_def( def ) || path_hint == 2;
		const auto lookup_def = static_cast< std::int16_t >( def );

		bool user_edited = false;
		if ( const auto it = settings::g_changer.skins.data.find( lookup_def ); it != settings::g_changer.skins.data.end( ) )
			user_edited = it->second.colors_edited;

		if ( !is_glove )
			lift_inject( colors, !user_edited );
		else if ( !user_edited )
			lift_inject( colors, false );

		append_color_vars( vector, colors, is_glove );
	}

	inline void on_weapon_mid( safetyhook::Context& ctx ) { on_build_material( ctx, 1 ); }
	inline void on_glove_mid( safetyhook::Context& ctx ) { on_build_material( ctx, 2 ); }

}

inline void restore_kit_colors( int paint_kit_id )
{
	detail::restore_kit_colors_impl( paint_kit_id );
}

inline void seed_from_paint_kit( int paint_kit_id, float out_rgba[ 16 ] )
{
	if ( !out_rgba )
		return;

	for ( int i = 0; i < 4; ++i )
	{
		out_rgba[ i * 4 + 0 ] = 1.f;
		out_rgba[ i * 4 + 1 ] = 1.f;
		out_rgba[ i * 4 + 2 ] = 1.f;
		out_rgba[ i * 4 + 3 ] = 1.f;
	}

	if ( paint_kit_id <= 0 )
		return;

	auto* snap = detail::ensure_snap( paint_kit_id );
	if ( !snap )
		return;

	if ( !snap->filler )
	{
		for ( int i = 0; i < 4; ++i )
		{
			out_rgba[ i * 4 + 0 ] = snap->raw[ i * 4 + 0 ] / 255.f;
			out_rgba[ i * 4 + 1 ] = snap->raw[ i * 4 + 1 ] / 255.f;
			out_rgba[ i * 4 + 2 ] = snap->raw[ i * 4 + 2 ] / 255.f;
			out_rgba[ i * 4 + 3 ] = 1.f;
		}
		return;
	}

	const float defaults[ 4 ][ 4 ] =
	{
		{ 0.90f, 0.20f, 0.20f, 1.f },
		{ 0.20f, 0.85f, 0.30f, 1.f },
		{ 0.20f, 0.35f, 0.95f, 1.f },
		{ 0.95f, 0.90f, 0.20f, 1.f }
	};

	const int rot = ( paint_kit_id > 0 ? paint_kit_id : 0 ) % 4;
	for ( int i = 0; i < 4; ++i )
	{
		const int s = ( i + rot ) % 4;
		out_rgba[ i * 4 + 0 ] = defaults[ s ][ 0 ];
		out_rgba[ i * 4 + 1 ] = defaults[ s ][ 1 ];
		out_rgba[ i * 4 + 2 ] = defaults[ s ][ 2 ];
		out_rgba[ i * 4 + 3 ] = 1.f;
	}
}

[[nodiscard]] inline bool ready( )
{
	return detail::g_ready;
}

[[nodiscard]] inline bool level_busy( )
{
	return detail::g_level_busy.load( std::memory_order_acquire );
}

struct rebuild_guard
{
	rebuild_guard( )
	{
		detail::g_rebuild_depth.fetch_add( 1, std::memory_order_acq_rel );
	}

	~rebuild_guard( )
	{
		detail::g_rebuild_depth.fetch_sub( 1, std::memory_order_acq_rel );
	}

	rebuild_guard( const rebuild_guard& ) = delete;
	rebuild_guard& operator=( const rebuild_guard& ) = delete;
};

inline void on_level_end( )
{
	detail::g_level_busy.store( true, std::memory_order_release );
	detail::g_map_playable.store( false, std::memory_order_release );
	detail::g_rebuild_depth.store( 0, std::memory_order_release );
	detail::g_spawned_at_ms.store( 0, std::memory_order_release );

	if ( !detail::get_schema( ) )
		return;

	for ( int i = 0; i < detail::snap_slots; ++i )
	{
		if ( detail::g_snaps[ i ].ok )
			detail::restore_kit_colors_impl( detail::g_snaps[ i ].id );
	}
}

inline void note_spawned( bool world_ready )
{
	if ( !detail::g_level_busy.load( std::memory_order_acquire ) )
		return;

	if ( !world_ready )
	{
		detail::g_spawned_at_ms.store( 0, std::memory_order_release );
		return;
	}

	const auto now = std::chrono::duration_cast< std::chrono::milliseconds >(
		std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );

	auto started = detail::g_spawned_at_ms.load( std::memory_order_acquire );
	if ( started <= 0 )
	{
		if ( !detail::g_spawned_at_ms.compare_exchange_strong( started, now, std::memory_order_acq_rel ) )
			started = detail::g_spawned_at_ms.load( std::memory_order_acquire );
		else
			started = now;
	}

	if ( now - started >= 2500 )
		detail::g_level_busy.store( false, std::memory_order_release );
}

inline void set_map_name( const char* new_map )
{
	bool playable = false;
	if ( new_map && *new_map )
	{
		const char* slash = std::strrchr( new_map, '/' );
		const char* bslash = std::strrchr( new_map, '\\' );
		const char* leaf = slash;
		if ( bslash && ( !leaf || bslash > leaf ) )
			leaf = bslash;
		const char* name = leaf ? leaf + 1 : new_map;
		playable = _strnicmp( name, "lobby", 5 ) != 0
			&& std::strstr( name, "background" ) == nullptr
			&& std::strcmp( name, "<empty>" ) != 0;
	}
	detail::g_map_playable.store( playable, std::memory_order_release );
}

[[nodiscard]] inline bool is_usable_ptr( std::uintptr_t address )
{
	return detail::usable_addr( address );
}

[[nodiscard]] inline bool entity_model_ready( std::uintptr_t entity )
{
	if ( !is_usable_ptr( entity ) )
		return false;

	const auto scene = reinterpret_cast< C_BaseEntity* >( entity )->m_pGameSceneNode( );
	if ( !scene || !is_usable_ptr( scene ) )
		return false;

	const auto model = reinterpret_cast< CSkeletonInstance* >( scene )->m_modelState( ).m_hModel( );
	if ( !model || !is_usable_ptr( model ) )
		return false;

	const auto resolved = memory::safe_read<std::uintptr_t>( model );
	if ( !resolved || !is_usable_ptr( *resolved ) )
		return false;

	const auto inner = memory::safe_read<std::uintptr_t>( *resolved + 0x10 );
	if ( inner && *inner && memory::is_game_ptr( *inner ) && !is_usable_ptr( *inner ) )
		return false;

	return true;
}

[[nodiscard]] inline bool session_playable( std::uintptr_t pawn )
{
	if ( !detail::g_map_playable.load( std::memory_order_acquire ) )
		return false;
	if ( !entity_model_ready( pawn ) )
		return false;
	const auto rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
	return rules && is_usable_ptr( rules );
}

inline std::uint32_t bump_item_serial( )
{
	static std::atomic<std::uint32_t> serial{ 0x20 };
	return serial.fetch_add( 1, std::memory_order_relaxed ) + 1;
}

inline void write_item_identity( std::uintptr_t iv, std::uint32_t account_id, std::uint32_t serial )
{
	if ( !iv )
		return;

	auto* item = reinterpret_cast< C_EconItemView* >( iv );
	item->m_iItemID( ) = ( 0xFFFFFFFFull << 32 ) | static_cast< std::uint64_t >( serial );
	item->m_iItemIDHigh( ) = 0xFFFFFFFFu;
	item->m_iItemIDLow( ) = serial;
	item->m_iAccountID( ) = account_id;
	item->m_bInitialized( ) = true;
	item->m_bRestoreCustomMaterialAfterPrecache( ) = true;
	item->m_iEntityQuality( ) = 0;
}

inline std::uintptr_t scene_node_of( std::uintptr_t entity )
{
	if ( !entity )
		return 0;

	return reinterpret_cast< C_BaseEntity* >( entity )->m_pGameSceneNode( );
}

inline std::string entity_model_path( std::uintptr_t entity )
{
	const auto scene = scene_node_of( entity );
	if ( !scene )
		return {};

	auto& model_state = reinterpret_cast< CSkeletonInstance* >( scene )->m_modelState( );
	const auto name_ptr = model_state.m_ModelName( );
	if ( !name_ptr )
		return {};

	return memory::read_string( name_ptr );
}

inline const char* model_stem( const char* path )
{
	if ( !path || !*path )
		return path;

	const char* stem = path;
	for ( const char* p = path; *p; ++p )
	{
		if ( *p == '/' || *p == '\\' )
			stem = p + 1;
	}

	return stem;
}

inline bool model_stem_matches( const char* current, const char* expected )
{
	if ( !current || !*current || !expected || !*expected )
		return false;

	const auto a = model_stem( current );
	const auto b = model_stem( expected );
	if ( !a || !b || !*b )
		return false;

	const auto n = std::strlen( b );
	return std::strncmp( a, b, n ) == 0;
}

inline bool entity_uses_model( std::uintptr_t entity, const char* expected )
{
	if ( !entity || !expected || !*expected )
		return false;

	const auto path = entity_model_path( entity );
	return !path.empty( ) && model_stem_matches( path.c_str( ), expected );
}

inline void set_entity_model( std::uintptr_t entity, const char* path )
{
	if ( !entity || !path || !*path )
		return;

	if ( const auto fn = PATTERN( PATTERN_SET_PLAYER_MODEL ) )
		memory::call<void>( fn, entity, path );
}

inline std::uintptr_t find_hud_weapon( std::uintptr_t pawn, std::uintptr_t weapon = 0 );
inline std::uintptr_t find_hud_weapon_for_path( std::uintptr_t pawn, const char* path );

inline void invalidate_composites( std::uintptr_t entity )
{
	if ( !entity_model_ready( entity ) )
		return;

	if ( const auto fn = PATTERN( PATTERN_WEAPON_UPDATE_COMPOSITE_MATERIAL ) )
		memory::call<void>( fn, entity + econ_schema::entity::composite_material, true );
}

inline void refresh_weapon_visuals( std::uintptr_t weapon, std::uintptr_t pawn = 0 )
{
	if ( !entity_model_ready( weapon ) )
		return;

	rebuild_guard guard{};

	reinterpret_cast< C_CSWeaponBase* >( weapon )->m_bVisualsDataSet( ) = false;
	invalidate_composites( weapon );

	if ( const auto update_skin = PATTERN( PATTERN_WEAPON_UPDATE_SKIN ) )
		memory::call<void>( update_skin, weapon, true );

	if ( pawn )
	{
		if ( const auto hud = find_hud_weapon( pawn, weapon ) )
			invalidate_composites( hud );
	}
}

inline void write_glove_identity( std::uintptr_t iv, std::uint32_t account_id, std::uint32_t serial )
{
	if ( !iv )
		return;

	auto* item = reinterpret_cast< C_EconItemView* >( iv );
	item->m_iItemID( ) = ( 0xFull << 32 ) | static_cast< std::uint64_t >( serial );
	item->m_iItemIDHigh( ) = 0xFu;
	item->m_iItemIDLow( ) = serial;
	item->m_iAccountID( ) = account_id;
	item->m_bInitialized( ) = true;
	item->m_bRestoreCustomMaterialAfterPrecache( ) = true;
	item->m_iEntityQuality( ) = 3;
	item->m_bDisallowSOC( ) = true;
}

inline bool scene_has_model( std::uintptr_t scene_node )
{
	if ( !is_usable_ptr( scene_node ) )
		return false;

	auto& model_state = reinterpret_cast< CSkeletonInstance* >( scene_node )->m_modelState( );
	const auto model = model_state.m_hModel( );
	return is_usable_ptr( model );
}

inline void set_scene_mesh_mask( std::uintptr_t scene_node, std::uint64_t mask )
{
	if ( !scene_has_model( scene_node ) )
		return;

	reinterpret_cast< CSkeletonInstance* >( scene_node )->m_modelState( ).m_MeshGroupMask( ) = mask;
}

inline void set_mesh_group_mask( std::uintptr_t entity, std::uint64_t mask )
{
	set_scene_mesh_mask( scene_node_of( entity ), mask );
}

inline void set_bodygroup( std::uintptr_t entity, int group, int value )
{
	if ( !entity_model_ready( entity ) )
		return;

	if ( const auto fn = PATTERN( PATTERN_SET_BODYGROUP ) )
		memory::call<void>( fn, entity, group, value );
}

[[nodiscard]] inline std::uint64_t mesh_mask_for( bool legacy )
{
	return legacy ? 2ull : 1ull;
}

inline std::uintptr_t find_hud_arms( std::uintptr_t pawn )
{
	if ( !pawn )
		return 0;

	const auto arms_handle = reinterpret_cast< C_CSPlayerPawn* >( pawn )->m_hHudModelArms( );
	if ( !arms_handle )
		return 0;

	return systems::g_entities.lookup( arms_handle );
}

inline std::uintptr_t viewmodel_attachment( std::uintptr_t weapon )
{
	if ( !weapon )
		return 0;

	const auto handle = reinterpret_cast< C_EconEntity* >( weapon )->m_hViewmodelAttachment( );
	if ( !handle )
		return 0;

	return systems::g_entities.lookup( handle );
}

inline bool path_looks_like_glove( const char* path )
{
	if ( !path || !*path )
		return false;

	for ( const char* p = path; *p; ++p )
	{
		const auto c = ( *p >= 'A' && *p <= 'Z' ) ? static_cast< char >( *p - 'A' + 'a' ) : *p;
		if ( c != 'g' )
			continue;
		if ( _strnicmp( p, "glove", 5 ) == 0 )
			return true;
	}

	return false;
}

template<typename Fn>
inline void for_each_class_in_scene( std::uintptr_t node, std::uint32_t class_hash, Fn&& fn, int depth = 0 )
{
	if ( !node || node < 0x10000 || depth > 8 )
		return;

	auto* scene = reinterpret_cast< CGameSceneNode* >( node );
	auto child = scene->m_pChild( );
	while ( child && child > 0x10000 )
	{
		const auto owner = reinterpret_cast< CGameSceneNode* >( child )->m_pOwner( );
		if ( owner && owner > 0x10000 )
		{
			const auto name = systems::g_entities.get_schema_name( owner );
			if ( name && fnv1a::runtime_hash( name ) == class_hash )
				fn( owner );
		}

		for_each_class_in_scene( child, class_hash, fn, depth + 1 );
		child = reinterpret_cast< CGameSceneNode* >( child )->m_pNextSibling( );
	}
}

template<typename Fn>
inline void for_each_scene_child( std::uintptr_t node, Fn&& fn, int depth = 0 )
{
	if ( !node || node < 0x10000 || depth > 8 )
		return;

	auto* scene = reinterpret_cast< CGameSceneNode* >( node );
	auto child = scene->m_pChild( );
	while ( child && child > 0x10000 )
	{
		const auto owner = reinterpret_cast< CGameSceneNode* >( child )->m_pOwner( );
		fn( child, owner );
		for_each_scene_child( child, fn, depth + 1 );
		child = reinterpret_cast< CGameSceneNode* >( child )->m_pNextSibling( );
	}
}

inline std::uintptr_t find_class_in_scene( std::uintptr_t node, std::uint32_t class_hash, int depth = 0 )
{
	std::uintptr_t found{};
	for_each_class_in_scene( node, class_hash, [ & ]( std::uintptr_t owner )
	{
		if ( !found )
			found = owner;
	}, depth );
	return found;
}

inline std::uintptr_t find_hud_weapon( std::uintptr_t pawn, std::uintptr_t weapon )
{
	const auto arms = find_hud_arms( pawn );
	if ( !arms )
		return 0;

	const auto expected = weapon ? entity_model_path( weapon ) : std::string{};
	std::uintptr_t first{};
	std::uintptr_t matched{};
	for_each_class_in_scene( scene_node_of( arms ), "C_CS2HudModelWeapon"_hash, [ & ]( std::uintptr_t owner )
	{
		if ( !first )
			first = owner;
		if ( !expected.empty( ) && entity_uses_model( owner, expected.c_str( ) ) )
			matched = owner;
	} );

	if ( matched )
		return matched;
	if ( weapon )
		return 0;
	return first;
}

inline std::uintptr_t find_hud_weapon_for_path( std::uintptr_t pawn, const char* path )
{
	const auto arms = find_hud_arms( pawn );
	if ( !arms || !path || !*path )
		return 0;

	std::uintptr_t matched{};
	for_each_class_in_scene( scene_node_of( arms ), "C_CS2HudModelWeapon"_hash, [ & ]( std::uintptr_t owner )
	{
		if ( entity_uses_model( owner, path ) )
			matched = owner;
	} );
	return matched;
}

inline std::uintptr_t find_world_gloves( std::uintptr_t pawn )
{
	if ( !pawn )
		return 0;

	if ( const auto from_pawn = find_class_in_scene( scene_node_of( pawn ), "C_WorldModelGloves"_hash ) )
		return from_pawn;

	const auto arms = find_hud_arms( pawn );
	if ( !arms )
		return 0;

	if ( const auto from_arms = find_class_in_scene( scene_node_of( arms ), "C_WorldModelGloves"_hash ) )
		return from_arms;

	std::uintptr_t addon{};
	for_each_class_in_scene( scene_node_of( arms ), "C_CS2HudModelAddon"_hash, [ & ]( std::uintptr_t owner )
	{
		if ( addon )
			return;
		const auto path = entity_model_path( owner );
		if ( path_looks_like_glove( path.c_str( ) ) )
			addon = owner;
	} );
	return addon;
}

inline void apply_weapon_mesh_mask( std::uintptr_t weapon, std::uintptr_t pawn, bool legacy )
{
	if ( !weapon )
		return;

	const auto mask = mesh_mask_for( legacy );
	set_mesh_group_mask( weapon, mask );

	if ( const auto attach = viewmodel_attachment( weapon ) )
		set_mesh_group_mask( attach, mask );

	if ( !pawn )
		return;

	const auto arms = find_hud_arms( pawn );
	if ( !arms )
		return;

	for_each_scene_child( scene_node_of( arms ), [ & ]( std::uintptr_t child, std::uintptr_t owner )
	{
		bool take = false;
		if ( owner && owner > 0x10000 )
		{
			const auto name = systems::g_entities.get_schema_name( owner );
			const auto hash = name ? fnv1a::runtime_hash( name ) : 0u;
			if ( hash == "C_CS2HudModelWeapon"_hash || hash == "C_CS2HudModelAddon"_hash )
				take = true;
			if ( reinterpret_cast< C_BaseEntity* >( owner )->m_hOwnerEntity( ) )
			{
				const auto handle = reinterpret_cast< C_BaseEntity* >( owner )->m_hOwnerEntity( );
				if ( handle && systems::g_entities.lookup( handle ) == weapon )
					take = true;
			}
		}

		if ( !take )
			return;

		set_scene_mesh_mask( child, mask );
		if ( owner )
			set_mesh_group_mask( owner, mask );
	} );
}

inline void pulse_glove_bodygroups( std::uintptr_t pawn )
{
	if ( !entity_model_ready( pawn ) )
		return;

	set_bodygroup( pawn, 0, 1 );
	if ( const auto gloves = find_world_gloves( pawn ) )
		set_bodygroup( gloves, 0, 1 );
}

inline void apply_hud_model( std::uintptr_t entity, const char* path )
{
	if ( !entity || !path || !*path )
		return;

	if ( !entity_uses_model( entity, path ) )
		set_entity_model( entity, path );
}

inline void apply_knife_models( std::uintptr_t weapon, std::uintptr_t pawn, const char* path, bool apply_hud, const char* hud_lookup = nullptr )
{
	if ( !path || !*path )
		return;

	apply_hud_model( weapon, path );
	apply_hud_model( viewmodel_attachment( weapon ), path );
	if ( !apply_hud )
		return;

	std::uintptr_t hud{};
	if ( hud_lookup && *hud_lookup )
		hud = find_hud_weapon_for_path( pawn, hud_lookup );
	else
	{
		hud = find_hud_weapon( pawn, weapon );
		if ( !hud )
			hud = find_hud_weapon( pawn );
	}
	apply_hud_model( hud, path );
}

inline bool knife_models_ready( std::uintptr_t weapon, std::uintptr_t pawn, const char* path, bool require_hud )
{
	if ( !path || !*path )
		return entity_model_ready( weapon );

	if ( !entity_uses_model( weapon, path ) )
		return false;

	if ( require_hud )
	{
		const auto hud = find_hud_weapon_for_path( pawn, path );
		if ( !hud )
			return false;
		return entity_uses_model( hud, path );
	}

	return true;
}

inline void apply_glove_visuals( std::uintptr_t pawn, const char* model_path, const char* original_arms_path )
{
	if ( !pawn )
		return;

	const auto arms = find_hud_arms( pawn );
	if ( arms )
	{
		const auto current = entity_model_path( arms );
		if ( path_looks_like_glove( current.c_str( ) ) && original_arms_path && *original_arms_path )
			apply_hud_model( arms, original_arms_path );
	}

	if ( model_path && *model_path )
	{
		if ( const auto gloves = find_world_gloves( pawn ) )
		{
			apply_hud_model( gloves, model_path );
			set_mesh_group_mask( gloves, 1ull );
			set_bodygroup( gloves, 0, 1 );
		}
	}

	pulse_glove_bodygroups( pawn );
}

inline void write_paint_attributes( std::uintptr_t iv, int paint_kit_id, int seed, float wear )
{
	if ( !iv )
		return;

	const auto set_attribute = PATTERN( PATTERN_ECON_ITEM_VIEW_SET_ATTRIBUTE );
	if ( !set_attribute )
		return;

	memory::call<void>( set_attribute, iv, xs( "set item texture prefab" ), static_cast< float >( paint_kit_id ) );
	memory::call<void>( set_attribute, iv, xs( "set item texture seed" ), static_cast< float >( seed ) );
	memory::call<void>( set_attribute, iv, xs( "set item texture wear" ), wear );
}

inline void prepare_kit( int paint_kit_id, const float* rgba16, bool edited )
{
	if ( paint_kit_id <= 0 || !rgba16 )
		return;

	detail::write_kit_colors( paint_kit_id, rgba16, edited );
}

inline bool install( )
{
	if ( detail::g_ready )
		return true;

	const auto weapon_mid = PATTERN( PATTERN_CUSTOM_PAINT_WEAPON_MID );
	if ( !weapon_mid )
		return false;

	auto result = safetyhook::MidHook::create( reinterpret_cast< void* >( weapon_mid ), &detail::on_weapon_mid );
	if ( !result )
		return false;
	detail::g_mid_weapon = std::move( *result );

	if ( const auto glove_mid = PATTERN( PATTERN_CUSTOM_PAINT_GLOVE_MID ) )
	{
		auto glove_result = safetyhook::MidHook::create( reinterpret_cast< void* >( glove_mid ), &detail::on_glove_mid );
		if ( glove_result )
			detail::g_mid_glove = std::move( *glove_result );
	}

	detail::g_ready = true;
	return true;
}

inline void uninstall( )
{
	detail::g_mid_weapon.reset( );
	detail::g_mid_glove.reset( );
	detail::g_ready = false;
}

}
