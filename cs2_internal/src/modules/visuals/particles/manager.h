#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

using particle_color = math::vector3;

struct particle_params
{
	float duration{};
	float scale{};
	float alpha{};
};

enum class particle_setting : int
{
	position = 0,
	info = 3,
	color = 16
};

struct particle_transform
{
	float px{};
	float py{};
	float pz{};
	float pw{};
	float qx{};
	float qy{};
	float qz{};
	float qw{ 1.0f };
};

struct particle_entity_binding
{
	std::intptr_t xy{ 0x7F7FFFFF7F7FFFFFll };
	int z{ 0x7F7FFFFF };
};

template <typename T>
class particle_snapshot_handle
{
public:
	T* data{};

	[[nodiscard]] T* get( ) const { return data; }
	[[nodiscard]] operator T*( ) const { return data; }
	[[nodiscard]] explicit operator bool( ) const { return data != nullptr; }
};

struct particle_snapshot_data
{
	enum attribute : std::size_t
	{
		position = 0,
		snapshot_time = 17,
		count = 24
	};

	std::array<void*, count> attributes{};

	void set_positions( math::vector3* positions )
	{
		attributes[ position ] = positions;
	}

	void set_times( float* values )
	{
		attributes[ snapshot_time ] = values;
	}
};

class c_particle_manager;
class c_client_particle_snapshot_system;

using particle_create_effect_fn = std::int64_t( __fastcall* )( c_particle_manager*, std::uint32_t*, const char*, int, std::int64_t, std::int64_t, std::int64_t, int );
using particle_set_effect_fn = void( __fastcall* )( c_particle_manager*, std::uint32_t, int, void*, int );
using particle_set_snapshot_fn = bool( __fastcall* )( c_particle_manager*, int, std::uint32_t, void* );
using particle_destroy_fn = void( __fastcall* )( c_particle_manager*, int, bool, bool );
using particle_set_control_point_fn = bool( __fastcall* )( c_particle_manager*, std::uint32_t, int, const math::vector3*, int );
using particle_set_entity_binding_fn = bool( __fastcall* )( c_particle_manager*, std::uint32_t, int, void*, int, void*, const particle_entity_binding*, int, std::int64_t );
using particle_set_transform_fn = bool( __fastcall* )( c_particle_manager*, std::uint32_t, int, const particle_transform*, int );

namespace particle_system_functions {

	inline particle_create_effect_fn create_effect{};
	inline particle_set_effect_fn set_effect{};
	inline particle_set_snapshot_fn set_snapshot{};
	inline particle_destroy_fn destroy{};
	inline particle_set_control_point_fn set_control_point{};
	inline particle_set_entity_binding_fn set_entity_binding{};
	inline particle_set_transform_fn set_transform{};

}

class c_client_particle_snapshot_system
{
public:
	particle_snapshot_handle<void>* create_snapshot( const particle_color& color, particle_snapshot_handle<void>* snapshot )
	{
		using fn = particle_snapshot_handle<void>*( __fastcall* )( c_client_particle_snapshot_system*, const particle_color*, particle_snapshot_handle<void>* );
		const auto fn_addr = memory::get_vfunc( reinterpret_cast< std::uintptr_t >( this ), 41 );
		if ( !fn_addr )
			return nullptr;

		return reinterpret_cast< fn >( fn_addr )( this, &color, snapshot );
	}

	void update_snapshot( void* snapshot, int count, particle_snapshot_data* data )
	{
		if ( !snapshot || !data )
			return;

		using fn = void( __fastcall* )( c_client_particle_snapshot_system*, void*, int, particle_snapshot_data* );
		const auto fn_addr = memory::get_vfunc( reinterpret_cast< std::uintptr_t >( this ), 42 );
		if ( !fn_addr )
			return;

		reinterpret_cast< fn >( fn_addr )( this, snapshot, count, data );
	}
};

class c_particle_manager
{
public:
	std::int64_t cache( std::int32_t* index, const char* name )
	{
		if ( !particle_system_functions::create_effect )
			return 0;

		const auto result = particle_system_functions::create_effect(
			this,
			reinterpret_cast< std::uint32_t* >( index ),
			name,
			2,
			0,
			0,
			0,
			0 );

		if ( index && *index != -1 && *index != 0 )
			return result;

		return particle_system_functions::create_effect(
			this,
			reinterpret_cast< std::uint32_t* >( index ),
			name,
			8,
			0,
			0,
			0,
			0 );
	}

	std::int64_t create( std::uint32_t* index, const char* name )
	{
		if ( !index || !particle_system_functions::create_effect )
			return 0;

		*index = 0;
		return particle_system_functions::create_effect(
			this,
			index,
			name,
			8,
			0,
			0,
			0,
			0 );
	}

	void set_effect( std::uint32_t index, particle_setting setting, void* value )
	{
		if ( particle_system_functions::set_effect )
			particle_system_functions::set_effect( this, index, static_cast< int >( setting ), value, 0 );
	}

	bool set_snapshot( std::uint32_t index, void* snapshot )
	{
		if ( !particle_system_functions::set_snapshot )
			return false;

		return particle_system_functions::set_snapshot( this, static_cast< int >( index ), 0u, snapshot );
	}

	void destroy( std::uint32_t index )
	{
		if ( particle_system_functions::destroy )
			particle_system_functions::destroy( this, static_cast< int >( index ), true, true );
	}

	void release( std::uint32_t index )
	{
		using release_fn = void( __fastcall* )( c_particle_manager*, int );
		const auto fn_addr = memory::get_vfunc( reinterpret_cast< std::uintptr_t >( this ), 3 );
		if ( !fn_addr )
			return;

		reinterpret_cast< release_fn >( fn_addr )( this, static_cast< int >( index ) );
	}
};

namespace particles {

	namespace detail {

		inline c_particle_manager* manager{};
		inline c_client_particle_snapshot_system* snapshot_system{};
		inline unsigned long long next_resolve_attempt_ms{};

		inline bool has_vfunc( const void* object, std::size_t index )
		{
			return memory::get_vfunc( reinterpret_cast< std::uintptr_t >( object ), index ) != 0;
		}

		inline constexpr std::size_t k_max_snapshot_points = 8;

		struct active_particle
		{
			std::uint32_t index{};
			particle_snapshot_handle<void> snapshot{};
			std::array<math::vector3, k_max_snapshot_points> positions{};
			std::array<float, k_max_snapshot_points> times{};
			particle_snapshot_data data{};
			std::size_t point_count{};
			bool has_times{};
			unsigned long long expire_time{};

			active_particle( ) = default;
			active_particle( const active_particle& ) = delete;
			active_particle& operator=( const active_particle& ) = delete;

			active_particle( active_particle&& other ) noexcept
			{
				*this = std::move( other );
			}

			active_particle& operator=( active_particle&& other ) noexcept
			{
				if ( this == &other )
					return *this;

				index = other.index;
				snapshot = other.snapshot;
				positions = other.positions;
				times = other.times;
				point_count = other.point_count;
				has_times = other.has_times;
				expire_time = other.expire_time;
				rebind_data( );

				other.index = 0;
				other.snapshot = {};
				other.point_count = 0;
				other.has_times = false;
				other.expire_time = 0;
				other.data = {};
				return *this;
			}

			void rebind_data( )
			{
				data = {};
				if ( point_count == 0 )
					return;

				data.set_positions( positions.data( ) );
				if ( has_times )
					data.set_times( times.data( ) );
			}
		};

		struct active_effect
		{
			std::int32_t effect_id{};
			unsigned long long expire_time{};
		};

		struct prewarmed_effect
		{
			const char* path{};
			std::int32_t effect_id{ -1 };
		};

		inline constexpr std::size_t k_max_active_particles = 96;
		inline constexpr std::size_t k_max_active_effects = 64;
		inline std::vector<active_particle> active_particles{};
		inline std::vector<active_effect> active_effects{};
		inline std::vector<prewarmed_effect> prewarmed_effects{};

		inline void reserve_runtime_storage( )
		{
			if ( active_particles.capacity( ) < k_max_active_particles )
				active_particles.reserve( k_max_active_particles );
			if ( active_effects.capacity( ) < k_max_active_effects )
				active_effects.reserve( k_max_active_effects );
			if ( prewarmed_effects.capacity( ) < 16 )
				prewarmed_effects.reserve( 16 );
		}

		inline bool is_prewarmed( const char* path )
		{
			if ( !path )
				return false;

			for ( const auto& warmed : prewarmed_effects )
			{
				if ( warmed.path && std::strcmp( warmed.path, path ) == 0 )
					return true;
			}

			return false;
		}

		inline bool is_finite( const math::vector3& v )
		{
			return std::isfinite( v.x ) && std::isfinite( v.y ) && std::isfinite( v.z );
		}

		inline c_particle_manager* resolve_manager( )
		{
			if ( !addresses::globals::particle_manager )
				return nullptr;

			const auto resolved = memory::read<c_particle_manager*>( addresses::globals::particle_manager );
			return has_vfunc( resolved, 3 ) ? resolved : nullptr;
		}

		inline c_client_particle_snapshot_system* resolve_snapshot_system( )
		{
			const auto instruction = PATTERN( PATTERN_CLIENT_PARTICLE_SNAPSHOT_SYSTEM );
			if ( !instruction )
				return nullptr;

			const auto resolved = memory::read<c_client_particle_snapshot_system*>( instruction );
			return has_vfunc( resolved, 41 ) && has_vfunc( resolved, 42 ) ? resolved : nullptr;
		}

		inline bool missing_required_systems( )
		{
			return !particle_system_functions::create_effect ||
				!particle_system_functions::set_effect ||
				!particle_system_functions::set_snapshot ||
				!particle_system_functions::destroy ||
				!manager ||
				!snapshot_system;
		}

		inline void ensure_functions( )
		{
			if ( !particle_system_functions::create_effect )
				particle_system_functions::create_effect = reinterpret_cast< particle_create_effect_fn >( PATTERN( PATTERN_PARTICLE_CREATE_EFFECT ) );

			if ( !particle_system_functions::set_effect )
				particle_system_functions::set_effect = reinterpret_cast< particle_set_effect_fn >( PATTERN( PATTERN_PARTICLE_SET_EFFECT ) );

			if ( !particle_system_functions::set_snapshot )
				particle_system_functions::set_snapshot = reinterpret_cast< particle_set_snapshot_fn >( PATTERN( PATTERN_PARTICLE_SET_SNAPSHOT ) );

			if ( !particle_system_functions::destroy )
				particle_system_functions::destroy = reinterpret_cast< particle_destroy_fn >( PATTERN( PATTERN_PARTICLE_DESTROY_EFFECT ) );

			if ( !particle_system_functions::set_control_point )
				particle_system_functions::set_control_point = reinterpret_cast< particle_set_control_point_fn >( PATTERN( PATTERN_PARTICLE_SET_CONTROL_POINT ) );

			if ( !particle_system_functions::set_entity_binding )
				particle_system_functions::set_entity_binding = reinterpret_cast< particle_set_entity_binding_fn >( PATTERN( PATTERN_PARTICLE_SET_ENTITY_BINDING ) );

			if ( !particle_system_functions::set_transform )
				particle_system_functions::set_transform = reinterpret_cast< particle_set_transform_fn >( PATTERN( PATTERN_PARTICLE_SET_TRANSFORM ) );
		}

		inline void ensure_systems( )
		{
			const auto now = GetTickCount64( );
			if ( next_resolve_attempt_ms && now < next_resolve_attempt_ms && missing_required_systems( ) )
				return;

			ensure_functions( );

			if ( !has_vfunc( manager, 3 ) )
				manager = resolve_manager( );

			if ( !has_vfunc( snapshot_system, 41 ) || !has_vfunc( snapshot_system, 42 ) )
				snapshot_system = resolve_snapshot_system( );

			next_resolve_attempt_ms = missing_required_systems( ) ? now + 1000ull : 0ull;
		}

		inline bool ready( )
		{
			ensure_systems( );
			return particle_system_functions::create_effect &&
				particle_system_functions::set_effect &&
				particle_system_functions::set_snapshot &&
				particle_system_functions::destroy &&
				has_vfunc( manager, 3 ) &&
				has_vfunc( snapshot_system, 41 ) &&
				has_vfunc( snapshot_system, 42 );
		}

		inline bool effects_ready( )
		{
			ensure_systems( );
			return particle_system_functions::create_effect &&
				particle_system_functions::set_effect &&
				particle_system_functions::destroy &&
				has_vfunc( manager, 3 );
		}

		inline bool can_use_manager( )
		{
			return has_vfunc( manager, 3 );
		}

		inline void* snapshot_object( const particle_snapshot_handle<void>& snapshot )
		{
			return snapshot.get( );
		}

		inline void destroy_active_particle( const active_particle& particle )
		{
			if ( !can_use_manager( ) )
				return;

			manager->destroy( particle.index );
			manager->release( particle.index );
		}

		inline void destroy_active_effect( const active_effect& effect )
		{
			if ( !can_use_manager( ) || !particle_system_functions::destroy )
				return;

			particle_system_functions::destroy( manager, effect.effect_id, true, true );
		}

	}

	inline bool ready( )
	{
		return detail::ready( );
	}

	inline bool effects_ready( )
	{
		return detail::effects_ready( );
	}

	inline void setup( )
	{
		detail::reserve_runtime_storage( );
		detail::ensure_systems( );
	}

	inline void clear( )
	{
		if ( detail::can_use_manager( ) )
		{
			for ( const auto& particle : detail::active_particles )
				detail::destroy_active_particle( particle );

			for ( const auto& effect : detail::active_effects )
				detail::destroy_active_effect( effect );

			for ( const auto& effect : detail::prewarmed_effects )
			{
				if ( effect.effect_id != -1 && particle_system_functions::destroy )
					particle_system_functions::destroy( detail::manager, effect.effect_id, true, true );
			}
		}

		detail::active_particles.clear( );
		detail::active_effects.clear( );
		detail::prewarmed_effects.clear( );
	}

	inline bool create( const char* path, std::uint32_t& index )
	{
		if ( !detail::effects_ready( ) || !path )
			return false;

		index = 0;
		detail::manager->create( &index, path );
		return index != 0;
	}

	inline bool cache_effect( const char* path, std::int32_t& effect_id )
	{
		if ( !detail::effects_ready( ) || !path )
			return false;

		effect_id = -1;
		detail::manager->cache( &effect_id, path );
		return effect_id != -1 && effect_id != 0;
	}

	inline bool spawn_cached( std::int32_t effect_id, const math::vector3& position )
	{
		if ( !detail::effects_ready( ) || effect_id == -1 )
			return false;

		using spawn_fn = char( __fastcall* )( c_particle_manager*, std::int32_t, std::int32_t, const math::vector3*, float );
		const auto spawn = reinterpret_cast< spawn_fn >( particle_system_functions::set_effect );
		return spawn( detail::manager, effect_id, 0, &position, 0.0f ) != 0 ||
			spawn( detail::manager, effect_id, 1, &position, 0.0f ) != 0;
	}

	inline void destroy_cached( std::int32_t effect_id )
	{
		if ( !detail::can_use_manager( ) || !particle_system_functions::destroy || effect_id == -1 )
			return;

		particle_system_functions::destroy( detail::manager, effect_id, true, true );
	}

	inline void prewarm( const char* path )
	{
		if ( !path || detail::is_prewarmed( path ) || !detail::effects_ready( ) )
			return;

		std::int32_t effect_id = -1;
		if ( !cache_effect( path, effect_id ) )
			return;

		detail::prewarmed_effects.push_back( { path, effect_id } );
	}

	inline void destroy_particle( std::uint32_t index )
	{
		if ( !detail::can_use_manager( ) || index == 0 )
			return;

		detail::manager->destroy( index );
		detail::manager->release( index );
	}

	inline void set_color( std::uint32_t index, const particle_color& color )
	{
		if ( !detail::effects_ready( ) )
			return;

		detail::manager->set_effect( index, particle_setting::color, const_cast< particle_color* >( &color ) );
	}

	inline bool set_control_point( std::uint32_t index, int point, const math::vector3& value )
	{
		if ( !detail::effects_ready( ) || !particle_system_functions::set_control_point || !detail::is_finite( value ) )
			return false;

		return particle_system_functions::set_control_point( detail::manager, index, point, &value, 0 );
	}

	inline bool set_entity_binding( std::uint32_t index, int point, void* entity )
	{
		if ( !detail::effects_ready( ) || !particle_system_functions::set_entity_binding || index == 0 || !entity )
			return false;

		static const particle_entity_binding default_binding_position{};
		return particle_system_functions::set_entity_binding(
			detail::manager,
			index,
			point,
			entity,
			1,
			nullptr,
			&default_binding_position,
			1,
			0 );
	}

	inline void set_params( std::uint32_t index, const particle_params& params )
	{
		if ( !detail::effects_ready( ) )
			return;

		detail::manager->set_effect( index, particle_setting::info, const_cast< particle_params* >( &params ) );
	}

	inline particle_snapshot_handle<void> create_snapshot( const particle_color& color )
	{
		if ( !detail::ready( ) )
			return {};

		particle_snapshot_handle<void> snapshot{};
		const auto created = detail::snapshot_system->create_snapshot( color, &snapshot );
		return created ? *created : snapshot;
	}

	inline bool attach_snapshot( std::uint32_t index, void* snapshot )
	{
		if ( !detail::ready( ) )
			return false;

		return detail::manager->set_snapshot( index, snapshot );
	}

	inline void* resolve_snapshot( const particle_snapshot_handle<void>& snapshot )
	{
		return detail::snapshot_object( snapshot );
	}

	inline void update_snapshot( void* snapshot, int count, particle_snapshot_data& data )
	{
		if ( !detail::ready( ) || !snapshot )
			return;

		detail::snapshot_system->update_snapshot( snapshot, count, &data );
	}

	inline void track_effect( std::int32_t effect_id, unsigned long long lifetime_ms )
	{
		if ( effect_id == -1 )
			return;

		detail::reserve_runtime_storage( );
		while ( detail::active_effects.size( ) >= detail::k_max_active_effects )
		{
			detail::destroy_active_effect( detail::active_effects.front( ) );
			detail::active_effects.erase( detail::active_effects.begin( ) );
		}

		detail::active_effects.push_back( { effect_id, GetTickCount64( ) + lifetime_ms } );
	}

	inline void destroy_tracked_effect( std::int32_t effect_id )
	{
		if ( !detail::can_use_manager( ) || !particle_system_functions::destroy || effect_id == -1 )
			return;

		for ( auto it = detail::active_effects.begin( ); it != detail::active_effects.end( ); ++it )
		{
			if ( it->effect_id == effect_id )
			{
				detail::destroy_active_effect( *it );
				detail::active_effects.erase( it );
				return;
			}
		}
	}

	inline bool spawn_one_shot( const char* path, const math::vector3& position, unsigned long long lifetime_ms, std::int32_t* spawned_effect_id = nullptr )
	{
		std::int32_t effect_id = -1;
		if ( !cache_effect( path, effect_id ) )
			return false;

		if ( !spawn_cached( effect_id, position ) )
		{
			destroy_cached( effect_id );
			return false;
		}

		track_effect( effect_id, lifetime_ms );
		if ( spawned_effect_id )
			*spawned_effect_id = effect_id;

		return true;
	}

	inline void update( )
	{
		detail::ensure_systems( );

		if ( !detail::can_use_manager( ) )
			return;

		const auto now = GetTickCount64( );
		for ( auto it = detail::active_particles.begin( ); it != detail::active_particles.end( ); )
		{
			if ( now >= it->expire_time )
			{
				detail::destroy_active_particle( *it );
				it = detail::active_particles.erase( it );
			}
			else
			{
				++it;
			}
		}

		for ( auto it = detail::active_effects.begin( ); it != detail::active_effects.end( ); )
		{
			if ( now >= it->expire_time )
			{
				detail::destroy_active_effect( *it );
				it = detail::active_effects.erase( it );
			}
			else
			{
				++it;
			}
		}
	}

	inline void reset( )
	{
		detail::active_particles.clear( );
		detail::active_effects.clear( );
		detail::prewarmed_effects.clear( );
		detail::manager = nullptr;
		detail::snapshot_system = nullptr;
		detail::next_resolve_attempt_ms = 0;
	}

	inline void destroy( )
	{
		clear( );
		detail::manager = nullptr;
		detail::snapshot_system = nullptr;
		detail::next_resolve_attempt_ms = 0;
		detail::prewarmed_effects.clear( );
		particle_system_functions::create_effect = nullptr;
		particle_system_functions::set_effect = nullptr;
		particle_system_functions::set_snapshot = nullptr;
		particle_system_functions::destroy = nullptr;
	}

}

