#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <core/common.hpp>

namespace features::misc {

	namespace {

		constexpr std::size_t k_max_snapshots{ 12 };
		constexpr std::size_t k_max_hitboxes{ 20 };
		constexpr float k_min_duration{ 0.05f };
		constexpr float k_max_duration{ 10.0f };
		constexpr float k_fade_fraction{ 0.35f };
		constexpr int k_circle_segments{ 24 };
		constexpr std::uint64_t k_duplicate_window_ms{ 100 };

		using captured_hitbox = onshot::captured_hitbox;
		using snapshot = onshot::snapshot;

		[[nodiscard]] math::vector3 transform_point( const captured_hitbox& hitbox, const math::vector3& local )
		{
			return hitbox.rotation.rotate_vector( local ) + hitbox.position;
		}

		[[nodiscard]] bool draw_world_line( xdraw::draw_list& draw_list, const math::vector3& start, const math::vector3& end, const xdraw::color& color )
		{
			const auto a = systems::g_view.project( start );
			const auto b = systems::g_view.project( end );
			if ( !systems::g_view.projection_valid( a ) || !systems::g_view.projection_valid( b ) )
			{
				return false;
			}

			const float line[ 4 ]{ a.x, a.y, b.x, b.y };
			draw_list.polyline( { line, 4 }, color, false, 1.0f );
			return true;
		}

		void draw_world_polyline( xdraw::draw_list& draw_list, const std::array<math::vector3, k_circle_segments>& points, int count, bool close, const xdraw::color& color )
		{
			if ( count < 2 )
			{
				return;
			}

			for ( auto i = 1; i < count; ++i )
			{
				( void )draw_world_line( draw_list, points[ i - 1 ], points[ i ], color );
			}

			if ( close )
			{
				( void )draw_world_line( draw_list, points[ count - 1 ], points[ 0 ], color );
			}
		}

		void build_basis( const math::vector3& axis, math::vector3& right, math::vector3& up )
		{
			const auto normal = axis.normalized( );
			const auto reference = std::fabs( normal.z ) < 0.85f
				? math::vector3{ 0.0f, 0.0f, 1.0f }
				: math::vector3{ 0.0f, 1.0f, 0.0f };

			right = normal.cross( reference ).normalized( );
			up = right.cross( normal ).normalized( );
		}

		void draw_circle( xdraw::draw_list& draw_list, const captured_hitbox& hitbox, const math::vector3& center, const math::vector3& local_right, const math::vector3& local_up, float radius, const xdraw::color& color )
		{
			std::array<math::vector3, k_circle_segments> points{};
			for ( auto i = 0; i < k_circle_segments; ++i )
			{
				const auto t = ( static_cast< float >( i ) / static_cast< float >( k_circle_segments ) ) * std::numbers::pi_v<float> * 2.0f;
				const auto offset = ( local_right * std::cos( t ) + local_up * std::sin( t ) ) * radius;
				points[ i ] = transform_point( hitbox, center + offset );
			}

			draw_world_polyline( draw_list, points, k_circle_segments, true, color );
		}

		void draw_arc( xdraw::draw_list& draw_list, const captured_hitbox& hitbox, const math::vector3& center, const math::vector3& local_axis, const math::vector3& local_radius_axis, float radius, bool positive_axis, const xdraw::color& color )
		{
			std::array<math::vector3, k_circle_segments> points{};
			const auto count = ( k_circle_segments / 2 ) + 1;
			const auto axis = positive_axis ? local_axis : -local_axis;

			for ( auto i = 0; i < count; ++i )
			{
				const auto t = ( static_cast< float >( i ) / static_cast< float >( count - 1 ) ) * std::numbers::pi_v<float>;
				const auto offset = ( local_radius_axis * std::cos( t ) + axis * std::sin( t ) ) * radius;
				points[ i ] = transform_point( hitbox, center + offset );
			}

			draw_world_polyline( draw_list, points, count, false, color );
		}

		void draw_sphere( xdraw::draw_list& draw_list, const captured_hitbox& hitbox, const math::vector3& center, float radius, const xdraw::color& color )
		{
			draw_circle( draw_list, hitbox, center, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, radius, color );
			draw_circle( draw_list, hitbox, center, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, radius, color );
			draw_circle( draw_list, hitbox, center, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, radius, color );
		}

		void draw_capsule( xdraw::draw_list& draw_list, const captured_hitbox& hitbox, const math::vector3& mins, const math::vector3& maxs, float radius, const xdraw::color& color )
		{
			const auto axis = maxs - mins;
			if ( axis.length_sqr( ) <= 0.0001f || radius <= 0.001f )
			{
				draw_sphere( draw_list, hitbox, mins, std::max( radius, 1.0f ), color );
				return;
			}

			const auto direction = axis.normalized( );
			math::vector3 right{}, up{};
			build_basis( axis, right, up );

			draw_circle( draw_list, hitbox, mins, right, up, radius, color );
			draw_circle( draw_list, hitbox, maxs, right, up, radius, color );

			const math::vector3 side_axes[ ]{ right, -right, up, -up };
			for ( const auto& side_axis : side_axes )
			{
				( void )draw_world_line(
					draw_list,
					transform_point( hitbox, mins + side_axis * radius ),
					transform_point( hitbox, maxs + side_axis * radius ),
					color );
			}

			draw_arc( draw_list, hitbox, mins, direction, right, radius, false, color );
			draw_arc( draw_list, hitbox, mins, direction, up, radius, false, color );
			draw_arc( draw_list, hitbox, maxs, direction, right, radius, true, color );
			draw_arc( draw_list, hitbox, maxs, direction, up, radius, true, color );
		}

		void draw_hitbox( xdraw::draw_list& draw_list, const captured_hitbox& hitbox, const xdraw::color& color )
		{
			if ( hitbox.radius > 0.001f )
			{
				draw_capsule( draw_list, hitbox, hitbox.mins, hitbox.maxs, hitbox.radius, color );
				return;
			}

			const math::vector3 corners[ 8 ]
			{
				{ hitbox.mins.x, hitbox.mins.y, hitbox.mins.z },
				{ hitbox.maxs.x, hitbox.mins.y, hitbox.mins.z },
				{ hitbox.maxs.x, hitbox.maxs.y, hitbox.mins.z },
				{ hitbox.mins.x, hitbox.maxs.y, hitbox.mins.z },
				{ hitbox.mins.x, hitbox.mins.y, hitbox.maxs.z },
				{ hitbox.maxs.x, hitbox.mins.y, hitbox.maxs.z },
				{ hitbox.maxs.x, hitbox.maxs.y, hitbox.maxs.z },
				{ hitbox.mins.x, hitbox.maxs.y, hitbox.maxs.z },
			};

			math::vector3 world[ 8 ]{};
			for ( auto i = 0; i < 8; ++i )
			{
				world[ i ] = transform_point( hitbox, corners[ i ] );
			}

			constexpr std::pair<int, int> edges[ 12 ]
			{
				{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
				{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
				{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
			};

			for ( const auto& [ a, b ] : edges )
			{
				( void )draw_world_line( draw_list, world[ a ], world[ b ], color );
			}
		}

		[[nodiscard]] float fade_alpha( const snapshot& snap, std::uint64_t now )
		{
			if ( now >= snap.expire_time )
			{
				return 0.0f;
			}

			const auto duration_ms = snap.expire_time > snap.create_time
				? snap.expire_time - snap.create_time
				: 1ull;
			const auto remaining_ms = snap.expire_time - now;
			const auto fade_window = std::max( 1.0f, static_cast< float >( duration_ms ) * k_fade_fraction );
			const auto fade = std::clamp( static_cast< float >( remaining_ms ) / fade_window, 0.0f, 1.0f );
			return fade * fade * ( 3.0f - 2.0f * fade );
		}

	}

	void onshot::clear( )
	{
		std::lock_guard lock( this->m_mutex );
		this->m_snapshots.clear( );
		this->m_last_victim = 0;
		this->m_last_snapshot_time = 0;
	}

	void onshot::on_player_hurt( std::uintptr_t event )
	{
		this->handle_event( event, true );
	}

	void onshot::on_player_death( std::uintptr_t event )
	{
		this->handle_event( event, false );
	}

	void onshot::handle_event( std::uintptr_t event, bool require_alive )
	{
		const auto& cfg = settings::g_misc.m_onshot;
		if ( !cfg.enabled.value || !event )
		{
			return;
		}

		const auto attacker_key = cstypes::event_hash{ 0, "attacker" };
		const auto userid_key = cstypes::event_hash{ 0, "userid" };

		const auto attacker = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), event, &attacker_key );
		const auto victim = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_CONTROLLER ), event, &userid_key );
		const auto local = systems::g_local.get( );

		if ( !attacker || attacker != local.controller || !victim || victim == local.controller )
		{
			return;
		}

		const auto victim_pawn = memory::call<std::uintptr_t>( PATTERN( PATTERN_GAME_EVENT_GET_PAWN ), event, &userid_key );
		if ( !victim_pawn )
		{
			return;
		}

		if ( require_alive )
		{
			const auto health = reinterpret_cast< C_BaseEntity* >( victim_pawn )->m_iHealth( );
			if ( health <= 0 )
			{
				return;
			}
		}

		const auto game_scene = reinterpret_cast< C_BaseEntity* >( victim_pawn )->m_pGameSceneNode( );
		if ( !game_scene )
		{
			return;
		}

		const auto hitbox_set = systems::g_hitboxes.query( game_scene );
		if ( hitbox_set.count <= 0 )
		{
			return;
		}

		const auto skeleton = systems::g_bones.get_skeleton( victim_pawn );
		snapshot snap{};

		for ( const auto& entry : hitbox_set )
		{
			if ( snap.count >= k_max_hitboxes || entry.bone < 0 || entry.bone >= static_cast< int >( skeleton.size( ) ) )
			{
				continue;
			}

			const auto& bone = skeleton[ entry.bone ];
			if ( bone.position.length_sqr( ) < 1.0f )
			{
				continue;
			}

			auto& captured = snap.hitboxes[ snap.count++ ];
			captured.position = bone.position;
			captured.rotation = bone.rotation;
			captured.mins = entry.mins;
			captured.maxs = entry.maxs;
			captured.radius = entry.radius;
			captured.valid = true;
		}

		if ( snap.count == 0 )
		{
			return;
		}

		const auto now = GetTickCount64( );
		if ( victim == this->m_last_victim && now >= this->m_last_snapshot_time && now - this->m_last_snapshot_time <= k_duplicate_window_ms )
		{
			return;
		}

		const auto duration = std::clamp( cfg.duration.value, k_min_duration, k_max_duration );
		snap.create_time = now;
		snap.expire_time = now + static_cast< std::uint64_t >( duration * 1000.0f );

		{
			std::lock_guard lock( this->m_mutex );
			if ( this->m_snapshots.size( ) >= k_max_snapshots )
			{
				this->m_snapshots.erase( this->m_snapshots.begin( ) );
			}

			this->m_snapshots.push_back( snap );
		}

		this->m_last_victim = victim;
		this->m_last_snapshot_time = now;
	}

	void onshot::on_render( xdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_misc.m_onshot;
		if ( !cfg.enabled.value )
		{
			this->clear( );
			return;
		}

		const auto now = GetTickCount64( );
		std::vector<snapshot> visible{};

		{
			std::lock_guard lock( this->m_mutex );
			visible.reserve( this->m_snapshots.size( ) );

			for ( auto it = this->m_snapshots.begin( ); it != this->m_snapshots.end( ); )
			{
				if ( now >= it->expire_time )
				{
					it = this->m_snapshots.erase( it );
					continue;
				}

				visible.push_back( *it );
				++it;
			}
		}

		const auto base = cfg.color.value;
		for ( const auto& snap : visible )
		{
			const auto alpha = std::clamp( ( base.a / 255.0f ) * fade_alpha( snap, now ), 0.0f, 1.0f );
			const auto color = xdraw::color{
				base.r,
				base.g,
				base.b,
				static_cast< std::uint8_t >( alpha * 255.0f )
			};

			for ( std::size_t i = 0; i < snap.count; ++i )
			{
				if ( snap.hitboxes[ i ].valid )
				{
					draw_hitbox( draw_list, snap.hitboxes[ i ], color );
				}
			}
		}
	}

	void onshot::on_level_change( )
	{
		this->clear( );
	}

}


