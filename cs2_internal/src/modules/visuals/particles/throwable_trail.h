#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/common.hpp>
#include <modules/visuals/particles/manager.h>
#include <modules/visuals/particles/vpcf.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace features::world {

	class throwable_trail
	{
	public:
		void on_frame_stage_notify( );
		void clear( );
		void forget( );

	private:
		static constexpr int k_max_samples = 96;
		static constexpr unsigned long long k_missing_grace_ms = 350;
		static constexpr unsigned long long k_throw_start_grace_ms = 450;
		static constexpr unsigned long long k_fade_duration_ms = 1800;

		struct trail_point
		{
			math::vector3 position{};
			unsigned long long time{};
		};

		struct trail
		{
			std::uintptr_t entity{};
			std::uint32_t particle_index{};
			particle_snapshot_handle<void> snapshot{};
			std::vector<trail_point> points{};
			unsigned long long last_seen{};
			unsigned long long fade_start{};
		};

		[[nodiscard]] static float fade_duration( ) { return static_cast< float >( k_fade_duration_ms ) * 0.001f; }
		[[nodiscard]] static bool is_finite( const math::vector3& v );
		[[nodiscard]] static bool is_valid_world_point( const math::vector3& point );
		[[nodiscard]] static bool is_throwable_weapon( std::uint16_t def_index );
		[[nodiscard]] particle_color trail_color( ) const;
		[[nodiscard]] float snapshot_span( const trail& trail ) const;
		void update_local_throw_start( unsigned long long now );
		[[nodiscard]] bool projectile_position( std::uintptr_t entity, math::vector3& out ) const;
		[[nodiscard]] trail* find_trail( std::uintptr_t entity );
		void destroy_trail( trail& trail );
		[[nodiscard]] bool create_trail( std::uintptr_t entity, const math::vector3& position, unsigned long long now );
		void add_point( trail& trail, const math::vector3& position, unsigned long long now );
		[[nodiscard]] float fade_alpha( const trail& trail, unsigned long long now ) const;
		void update_snapshot( trail& trail, unsigned long long now );

		std::vector<trail> m_trails{};
		math::vector3 m_last_throw_start{};
		unsigned long long m_last_throw_start_time{};
	};

	inline bool throwable_trail::is_finite( const math::vector3& v )
	{
		return std::isfinite( v.x ) && std::isfinite( v.y ) && std::isfinite( v.z );
	}

	inline bool throwable_trail::is_valid_world_point( const math::vector3& point )
	{
		return is_finite( point ) && point.length_sqr( ) > 1.0f;
	}

	inline bool throwable_trail::is_throwable_weapon( std::uint16_t def_index )
	{
		using namespace cstypes::item_definition_index;
		switch ( def_index )
		{
		case weapon_flashbang:
		case weapon_high_explosive_grenade:
		case weapon_smoke_grenade:
		case weapon_molotov:
		case weapon_decoy_grenade:
		case weapon_incendiary_grenade:
			return true;
		default:
			return false;
		}
	}

	inline particle_color throwable_trail::trail_color( ) const
	{
		const auto floats = settings::g_world.m_particles.throwable_trail_color.value.to_float( );
		return { floats[ 0 ] * 255.0f, floats[ 1 ] * 255.0f, floats[ 2 ] * 255.0f };
	}

	inline float throwable_trail::snapshot_span( const trail& trail ) const
	{
		if ( trail.points.size( ) < 2 )
			return fade_duration( );

		const auto first = trail.points.front( ).time;
		const auto last = trail.points.back( ).time;
		const auto span = static_cast< float >( last - first ) * 0.001f;
		return std::clamp( span + 0.10f, fade_duration( ), 10.0f );
	}

	inline void throwable_trail::update_local_throw_start( unsigned long long now )
	{
		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn )
			return;

		const auto weapon_services = reinterpret_cast< C_BasePlayerPawn* >( local.pawn )->m_pWeaponServices( );
		if ( !weapon_services )
			return;

		const auto weapon_handle = reinterpret_cast< CPlayer_WeaponServices* >( weapon_services )->m_hActiveWeapon( );
		const auto weapon = systems::g_entities.lookup( weapon_handle );
		if ( !weapon )
			return;

		const auto def_index = memory::read<std::uint16_t>(
			weapon + SCHEMA_OFFSET( "C_EconEntity", "m_AttributeManager"_hash ) +
			SCHEMA_OFFSET( "C_AttributeContainer", "m_Item"_hash ) +
			SCHEMA_OFFSET( "C_EconItemView", "m_iItemDefinitionIndex"_hash ) );

		if ( !is_throwable_weapon( def_index ) )
			return;

		const auto scene_node = reinterpret_cast< C_BaseEntity* >( local.pawn )->m_pGameSceneNode( );
		if ( !scene_node )
			return;

		const auto eye = reinterpret_cast< CGameSceneNode* >( scene_node )->m_vecAbsOrigin( ) +
			reinterpret_cast< C_BaseModelEntity* >( local.pawn )->m_vecViewOffset( );

		if ( !is_valid_world_point( eye ) )
			return;

		this->m_last_throw_start = eye;
		this->m_last_throw_start_time = now;
	}

	inline bool throwable_trail::projectile_position( std::uintptr_t entity, math::vector3& out ) const
	{
		const auto scene_node = reinterpret_cast< C_BaseEntity* >( entity )->m_pGameSceneNode( );
		if ( !scene_node )
			return false;

		out = reinterpret_cast< CGameSceneNode* >( scene_node )->m_vecAbsOrigin( );
		return is_valid_world_point( out );
	}

	inline throwable_trail::trail* throwable_trail::find_trail( std::uintptr_t entity )
	{
		for ( auto& trail : this->m_trails )
		{
			if ( trail.entity == entity )
				return &trail;
		}

		return nullptr;
	}

	inline void throwable_trail::destroy_trail( trail& trail )
	{
		particles::destroy_particle( trail.particle_index );
		trail = {};
	}

	inline bool throwable_trail::create_trail( std::uintptr_t entity, const math::vector3& position, unsigned long long now )
	{
		if ( !particles::ready( ) || !entity )
			return false;

		std::uint32_t particle_index = 0;
		if ( !particles::create( vpcf::throwable_trail::spectator_utility_trail, particle_index ) )
			return false;

		const auto color = this->trail_color( );
		const auto alpha = settings::g_world.m_particles.throwable_trail_color.value.to_float( )[ 3 ];
		const particle_params params{ 10.0f, 1.35f, alpha };

		particles::set_color( particle_index, color );
		particles::set_params( particle_index, params );

		const auto snapshot = particles::create_snapshot( color );
		const auto snapshot_object = particles::resolve_snapshot( snapshot );
		if ( !snapshot_object )
		{
			particles::destroy_particle( particle_index );
			return false;
		}

		if ( !particles::attach_snapshot( particle_index, snapshot_object ) )
		{
			particles::destroy_particle( particle_index );
			return false;
		}

		trail trail{};
		trail.entity = entity;
		trail.particle_index = particle_index;
		trail.snapshot = snapshot;
		trail.last_seen = now;
		trail.points.reserve( k_max_samples );

		if ( this->m_last_throw_start_time &&
			now - this->m_last_throw_start_time <= k_throw_start_grace_ms &&
			is_valid_world_point( this->m_last_throw_start ) &&
			this->m_last_throw_start.distance_sqr( position ) < 512.0f * 512.0f )
		{
			trail.points.push_back( { this->m_last_throw_start, this->m_last_throw_start_time } );
		}

		trail.points.push_back( { position, now } );
		this->m_trails.push_back( std::move( trail ) );
		return true;
	}

	inline void throwable_trail::add_point( trail& trail, const math::vector3& position, unsigned long long now )
	{
		if ( !is_valid_world_point( position ) )
			return;

		trail.last_seen = now;

		if ( !trail.points.empty( ) && trail.points.back( ).position.distance_sqr( position ) < 6.0f * 6.0f )
		{
			trail.points.back( ).position = position;
			trail.points.back( ).time = now;
			return;
		}

		trail.points.push_back( { position, now } );
		if ( trail.points.size( ) > k_max_samples )
			trail.points.erase( trail.points.begin( ) + 1 );
	}

	inline float throwable_trail::fade_alpha( const trail& trail, unsigned long long now ) const
	{
		if ( !trail.fade_start )
			return 1.0f;

		const auto elapsed = static_cast< float >( now - trail.fade_start );
		return std::clamp( 1.0f - ( elapsed / static_cast< float >( k_fade_duration_ms ) ), 0.0f, 1.0f );
	}

	inline void throwable_trail::update_snapshot( trail& trail, unsigned long long now )
	{
		const auto snapshot_object = particles::resolve_snapshot( trail.snapshot );
		if ( !snapshot_object || trail.points.size( ) < 2 )
			return;

		const auto color = this->trail_color( );
		particles::set_color( trail.particle_index, color );

		const auto base_alpha = settings::g_world.m_particles.throwable_trail_color.value.to_float( )[ 3 ];
		const particle_params params{
			this->snapshot_span( trail ),
			1.35f,
			base_alpha * this->fade_alpha( trail, now )
		};
		particles::set_params( trail.particle_index, params );

		std::array<math::vector3, k_max_samples> positions{};
		std::array<float, k_max_samples> times{};

		const auto count = static_cast< int >( ( std::min )( trail.points.size( ), positions.size( ) ) );
		const auto start_time = trail.points[ trail.points.size( ) - count ].time;

		for ( auto i = 0; i < count; ++i )
		{
			const auto& point = trail.points[ trail.points.size( ) - count + i ];
			positions[ i ] = point.position;
			times[ i ] = static_cast< float >( point.time - start_time ) * 0.001f;
		}

		particle_snapshot_data data{};
		data.set_positions( positions.data( ) );
		data.set_times( times.data( ) );
		particles::update_snapshot( snapshot_object, count, data );
	}

	inline void throwable_trail::clear( )
	{
		for ( auto& trail : this->m_trails )
			this->destroy_trail( trail );

		this->m_trails.clear( );
	}

	inline void throwable_trail::forget( )
	{
		this->m_trails.clear( );
	}

	inline void throwable_trail::on_frame_stage_notify( )
	{
		if ( !settings::g_world.m_particles.throwable_trail.value || !particles::ready( ) )
		{
			this->clear( );
			return;
		}

		const auto now = GetTickCount64( );
		this->update_local_throw_start( now );

		std::vector<std::uintptr_t> projectiles{};
		for ( const auto& entry : systems::g_entities.get_by_type( systems::entities::type::projectile ) )
		{
			if ( entry.schema_hash == "C_Inferno"_hash )
				continue;

			projectiles.push_back( entry.ptr );
		}

		for ( const auto entity : projectiles )
		{
			math::vector3 position{};
			if ( !this->projectile_position( entity, position ) )
				continue;

			auto* trail = this->find_trail( entity );
			if ( !trail )
			{
				static_cast<void>( this->create_trail( entity, position, now ) );
				continue;
			}

			this->add_point( *trail, position, now );
		}

		for ( auto it = this->m_trails.begin( ); it != this->m_trails.end( ); )
		{
			auto& trail = *it;
			const auto missing = std::find( projectiles.begin( ), projectiles.end( ), trail.entity ) == projectiles.end( );
			if ( missing && now - trail.last_seen > k_missing_grace_ms )
			{
				trail.entity = 0;
				if ( !trail.fade_start )
					trail.fade_start = now;
			}

			if ( !trail.entity && trail.fade_start && now - trail.fade_start >= k_fade_duration_ms )
			{
				this->destroy_trail( trail );
				it = this->m_trails.erase( it );
				continue;
			}

			this->update_snapshot( trail, now );
			++it;
		}
	}

	inline throwable_trail g_throwable_trail{};

} // namespace features::world


