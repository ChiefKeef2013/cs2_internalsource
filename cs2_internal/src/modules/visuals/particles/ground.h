#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/common.hpp>
#include <modules/visuals/particles/manager.h>
#include <modules/visuals/particles/vpcf.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace features::world {

	class ground_particles
	{
	public:
		enum class placement : int
		{
			cached_ring,
			static_ring,
			burst_ring,
			burst_feet
		};

		enum class preset : int
		{
			embers_circle = 0,
			sensor_debris,
			waterfall_base,
			waterfall_anubis,
			sparks_glow,
			dust_embers,
			env_fire_tiny,
			count
		};

		struct preset_info
		{
			const char* label;
			const char* effect;
			placement placement;
			int emitters;
			float radius;
			float height;
			unsigned long long interval_ms;
			unsigned long long lifetime_ms;
		};

		static constexpr preset_info presets[] = {
			{ "embers circle", vpcf::ground::embers, placement::cached_ring, 5, 24.0f, 2.0f, 65, 0 },
			{ "sensor debris", vpcf::ground::sensor_debris, placement::burst_ring, 6, 42.0f, 5.0f, 180, 900 },
			{ "waterfall base", vpcf::ground::waterfall_base, placement::cached_ring, 5, 42.0f, 3.0f, 90, 0 },
			{ "waterfall anubis", vpcf::ground::waterfall_anubis, placement::cached_ring, 5, 42.0f, 3.0f, 90, 0 },
			{ "sparks glow", vpcf::ground::sparks_glow, placement::burst_ring, 6, 34.0f, 4.0f, 200, 900 },
			{ "dust embers", vpcf::ground::dust_embers, placement::cached_ring, 5, 30.0f, 3.0f, 75, 0 },
			{ "tiny fire", vpcf::ground::env_fire_tiny, placement::burst_ring, 5, 22.0f, 2.0f, 220, 1100 }
		};

		static constexpr int preset_count = static_cast< int >( preset::count );
		static constexpr const char* preset_labels[] = {
			"embers circle",
			"sensor debris",
			"waterfall base",
			"waterfall anubis",
			"sparks glow",
			"dust embers",
			"tiny fire"
		};

		void on_frame_stage_notify( );
		void reset( );
		void disable( );

	private:
		struct follow_effect
		{
			std::int32_t effect_id;
			unsigned long long expire_time;
		};

		static constexpr float spin_speed = 4.0f;

		void clear_state( );
		void reset_internal( bool destroy_effects );
		void disable_internal( bool destroy_effects );
		[[nodiscard]] int current_preset( ) const;
		[[nodiscard]] bool cache_effect( int preset );
		[[nodiscard]] bool feet_origin( math::vector3& origin ) const;
		void update_orbit( );
		[[nodiscard]] bool should_spawn( const preset_info& preset );
		void prune_follow_effects( );
		void spawn_ring( const math::vector3& feet );
		void spawn_static_ring( const math::vector3& feet );
		void spawn_at_feet( const math::vector3& feet );
		[[nodiscard]] static bool uses_cached_effect( const preset_info& preset );

		std::int32_t m_cached_effect_id{ -1 };
		int m_cached_preset{ -1 };
		unsigned long long m_last_time{};
		unsigned long long m_next_spawn_time{};
		float m_orbit_phase{};
		bool m_enabled_last_frame{};
		std::vector<follow_effect> m_follow_effects{};
	};

	inline void ground_particles::clear_state( )
	{
		this->m_cached_effect_id = -1;
		this->m_cached_preset = -1;
		this->m_last_time = 0;
		this->m_next_spawn_time = 0;
		this->m_orbit_phase = 0.0f;
		this->m_follow_effects.clear( );
	}

	inline void ground_particles::reset_internal( bool destroy_effects )
	{
		const auto effect_id = this->m_cached_effect_id;
		this->m_cached_effect_id = -1;

		if ( destroy_effects )
		{
			particles::destroy_cached( effect_id );
			for ( const auto& effect : this->m_follow_effects )
				particles::destroy_tracked_effect( effect.effect_id );
		}

		this->clear_state( );
	}

	inline void ground_particles::disable_internal( bool destroy_effects )
	{
		if ( this->m_enabled_last_frame || this->m_cached_effect_id != -1 || !this->m_follow_effects.empty( ) )
			this->reset_internal( destroy_effects );

		this->m_enabled_last_frame = false;
	}

	inline int ground_particles::current_preset( ) const
	{
		const auto preset = settings::g_world.m_particles.ground_preset.value;
		if ( preset < 0 || preset >= preset_count )
			return static_cast< int >( preset::embers_circle );

		return preset;
	}

	inline bool ground_particles::cache_effect( int preset )
	{
		if ( this->m_cached_effect_id != -1 && this->m_cached_preset == preset )
			return true;

		this->reset_internal( particles::effects_ready( ) );
		if ( !particles::cache_effect( presets[ preset ].effect, this->m_cached_effect_id ) )
			return false;

		this->m_cached_preset = preset;
		this->m_next_spawn_time = 0;
		return true;
	}

	inline bool ground_particles::feet_origin( math::vector3& origin ) const
	{
		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn )
			return false;

		const auto scene_node = reinterpret_cast< C_BaseEntity* >( local.pawn )->m_pGameSceneNode( );
		if ( !scene_node )
			return false;

		origin = reinterpret_cast< CGameSceneNode* >( scene_node )->m_vecAbsOrigin( );
		const auto collision = reinterpret_cast< C_BaseEntity* >( local.pawn )->m_pCollision( );
		origin.z += collision ? reinterpret_cast< CCollisionProperty* >( collision )->m_vecMins( ).z + 2.0f : 2.0f;
		return std::isfinite( origin.x ) && std::isfinite( origin.y ) && std::isfinite( origin.z );
	}

	inline void ground_particles::update_orbit( )
	{
		constexpr auto two_pi = 6.28318530718f;
		const auto now = GetTickCount64( );
		const auto frame_time = this->m_last_time ? static_cast< float >( now - this->m_last_time ) * 0.001f : 0.016f;
		this->m_last_time = now;
		this->m_orbit_phase += frame_time * spin_speed;
		if ( this->m_orbit_phase > two_pi )
			this->m_orbit_phase -= two_pi;
	}

	inline bool ground_particles::should_spawn( const preset_info& preset )
	{
		const auto now = GetTickCount64( );
		if ( now < this->m_next_spawn_time )
			return false;

		this->m_next_spawn_time = now + preset.interval_ms;
		return true;
	}

	inline void ground_particles::prune_follow_effects( )
	{
		const auto now = GetTickCount64( );
		for ( auto it = this->m_follow_effects.begin( ); it != this->m_follow_effects.end( ); )
		{
			if ( now >= it->expire_time )
				it = this->m_follow_effects.erase( it );
			else
				++it;
		}
	}

	inline bool ground_particles::uses_cached_effect( const preset_info& preset )
	{
		return preset.placement == placement::cached_ring || preset.placement == placement::static_ring;
	}

	inline void ground_particles::spawn_ring( const math::vector3& feet )
	{
		if ( this->m_cached_effect_id == -1 || this->m_cached_preset < 0 || this->m_cached_preset >= preset_count )
			return;

		constexpr auto two_pi = 6.28318530718f;
		const auto& preset = presets[ this->m_cached_preset ];
		for ( auto i = 0; i < preset.emitters; ++i )
		{
			const auto t = static_cast< float >( i ) / static_cast< float >( preset.emitters );
			const auto angle = two_pi * t + this->m_orbit_phase;
			math::vector3 point = feet;
			point.x += std::cos( angle ) * preset.radius;
			point.y += std::sin( angle ) * preset.radius;
			point.z += preset.height;
			if ( std::isfinite( point.x ) )
				particles::spawn_cached( this->m_cached_effect_id, point );
		}
	}

	inline void ground_particles::spawn_static_ring( const math::vector3& feet )
	{
		if ( this->m_cached_effect_id == -1 || this->m_cached_preset < 0 || this->m_cached_preset >= preset_count )
			return;

		constexpr auto two_pi = 6.28318530718f;
		const auto& preset = presets[ this->m_cached_preset ];
		for ( auto i = 0; i < preset.emitters; ++i )
		{
			const auto angle = two_pi * static_cast< float >( i ) / static_cast< float >( preset.emitters );
			math::vector3 point = feet;
			point.x += std::cos( angle ) * preset.radius;
			point.y += std::sin( angle ) * preset.radius;
			point.z += preset.height;
			if ( std::isfinite( point.x ) )
				particles::spawn_cached( this->m_cached_effect_id, point );
		}
	}

	inline void ground_particles::spawn_at_feet( const math::vector3& feet )
	{
		if ( this->m_cached_preset < 0 || this->m_cached_preset >= preset_count )
			return;

		const auto& preset = presets[ this->m_cached_preset ];
		constexpr auto two_pi = 6.28318530718f;
		for ( auto i = 0; i < preset.emitters; ++i )
		{
			auto angle = two_pi * static_cast< float >( i ) / static_cast< float >( preset.emitters );
			if ( preset.placement == placement::burst_ring )
				angle += this->m_orbit_phase;

			math::vector3 point = feet;
			point.x += std::cos( angle ) * preset.radius;
			point.y += std::sin( angle ) * preset.radius;
			point.z += preset.height;

			std::int32_t effect_id = -1;
			if ( std::isfinite( point.x ) && particles::spawn_one_shot( preset.effect, point, preset.lifetime_ms, &effect_id ) )
				this->m_follow_effects.push_back( { effect_id, GetTickCount64( ) + preset.lifetime_ms } );
		}
	}

	inline void ground_particles::on_frame_stage_notify( )
	{
		if ( !settings::g_world.m_particles.ground.value )
		{
			this->disable_internal( true );
			return;
		}

		if ( !particles::effects_ready( ) )
		{
			this->disable_internal( false );
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.is_alive )
		{
			this->disable_internal( true );
			return;
		}

		this->prune_follow_effects( );

		math::vector3 feet{};
		const auto preset = this->current_preset( );
		if ( !this->feet_origin( feet ) )
			return;

		this->m_enabled_last_frame = true;

		if ( this->m_cached_preset != -1 && this->m_cached_preset != preset )
			this->reset_internal( true );

		const auto& preset_info = presets[ preset ];
		if ( uses_cached_effect( preset_info ) )
		{
			if ( !this->cache_effect( preset ) )
				return;

			if ( preset_info.placement == placement::cached_ring )
				this->update_orbit( );
		}
		else if ( preset_info.placement == placement::burst_ring )
		{
			this->update_orbit( );
		}
		else if ( this->m_cached_effect_id != -1 )
		{
			this->reset_internal( true );
		}

		this->m_cached_preset = preset;

		if ( this->should_spawn( preset_info ) )
		{
			if ( preset_info.placement == placement::cached_ring )
				this->spawn_ring( feet );
			else if ( preset_info.placement == placement::static_ring )
				this->spawn_static_ring( feet );
			else
				this->spawn_at_feet( feet );
		}
	}

	inline void ground_particles::reset( )
	{
		this->reset_internal( false );
		this->m_enabled_last_frame = false;
	}

	inline void ground_particles::disable( )
	{
		this->disable_internal( particles::effects_ready( ) );
	}

	inline ground_particles g_ground_particles{};

} // namespace features::world
