#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>

namespace features::combat {

	inline std::array<systems::bones::data, 128>& applied_bone_scratch( )
	{
		thread_local std::array<systems::bones::data, 128> scratch{};
		return scratch;
	}

	bool shared::lagcomp::record::setup( std::uintptr_t pawn )
	{
		if ( !memory::is_game_ptr( pawn ) )
		{
			return false;
		}

		this->pawn = pawn;
		this->game_scene_node = reinterpret_cast<C_BaseEntity*>(pawn)->m_pGameSceneNode();

		if ( !memory::is_game_ptr( this->game_scene_node ) )
		{
			return false;
		}

		this->bone_cache = memory::read<std::uintptr_t>( this->game_scene_node + SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash ) + 0x80 );
		if ( !memory::is_game_ptr( this->bone_cache ) )
		{
			return false;
		}

		this->bone_count = memory::read<int>( this->game_scene_node + SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash ) + 0x8c );
		if ( this->bone_count <= 0 )
		{
			return false;
		}
		this->bone_count = std::min( this->bone_count, 128 );

		const auto scene_node = reinterpret_cast<CGameSceneNode*>(this->game_scene_node);
		const auto abs_origin = scene_node->m_vecAbsOrigin();
		const auto abs_rotation = scene_node->m_angAbsRotation();
		if ( !std::isfinite( abs_origin.x ) || !std::isfinite( abs_origin.y ) || !std::isfinite( abs_origin.z ) )
		{
			return false;
		}

		this->origin = abs_origin;
		this->rotation = abs_rotation;
		this->velocity = reinterpret_cast<C_BaseEntity*>( pawn )->m_vecVelocity();
		this->flags = reinterpret_cast<C_BaseEntity*>( pawn )->m_fFlags();
		this->gravity_scale = reinterpret_cast<C_BaseEntity*>( pawn )->m_flGravityScale();

		this->simulation_time = reinterpret_cast<C_BaseEntity*>(pawn)->m_flSimulationTime();

		if ( const auto movement_services = reinterpret_cast<C_BasePlayerPawn*>( pawn )->m_pMovementServices() )
		{
			this->surface_friction =
				reinterpret_cast<CPlayer_MovementServices_Humanoid*>( movement_services )->m_flSurfaceFriction();
		}

		std::memcpy( this->bones, reinterpret_cast<void*>( this->bone_cache ), sizeof( systems::bones::data ) * this->bone_count );

		this->tick = cstypes::time_to_ticks( this->simulation_time );
		this->valid = true;

		return true;
	}

	struct lagcomp_budget_cache
	{
		std::uint32_t gen{ ~0u };
		int now_tick{};
		int budget_ticks{};
		bool valid{};
	};

	constexpr int k_lagcomp_safety_ticks{ 2 };

	inline std::atomic<std::uint32_t>& lc_budget_generation( )
	{
		static std::atomic<std::uint32_t> gen{ 0 };
		return gen;
	}

	inline lagcomp_budget_cache& lc_budget_cache( )
	{
		thread_local lagcomp_budget_cache cache{};
		return cache;
	}

	inline void compute_lc_budget( lagcomp_budget_cache& cache, std::uint32_t gen )
	{
		cache.gen = gen;
		cache.valid = false;

		const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );
		if ( !global_vars )
		{
			return;
		}

		const auto server_limit_cvar = CONVAR( "sv_maxunlag" );
		const auto player_limit_cvar = CONVAR( "sv_maxunlag_player" );
		if ( !server_limit_cvar )
		{
			return;
		}

		const auto server_limit = server_limit_cvar->get<float>( );
		const auto player_limit = player_limit_cvar ? player_limit_cvar->get<float>( ) : 0.0f;
		const auto max_unlag = player_limit > 0.0f ? std::min( server_limit, player_limit ) : server_limit;

		if ( !std::isfinite( max_unlag ) )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !local.controller )
		{
			return;
		}

		const auto tick_base = reinterpret_cast<CBasePlayerController*>( local.controller )->m_nTickBase( );
		if ( tick_base <= 0 )
		{
			return;
		}

		auto now_tick = static_cast<int>( tick_base );
		auto latency_ticks = 0;
		if ( const auto net_client = addresses::globals::network_client_service )
		{
			if ( const auto tick_state = memory::call_vfunc<std::uintptr_t>( net_client, cstypes::offsets::net_tick_info_vfunc ) )
			{
				const auto server_tick = memory::read<int>( tick_state + cstypes::offsets::net_server_tick );
				if ( server_tick > 0 )
				{
					now_tick = std::max<int>( static_cast<int>( tick_base ), server_tick );
					latency_ticks = std::max( 0, server_tick - static_cast<int>( tick_base ) );
				}
			}
		}

		const auto dynamic_safety = std::max( k_lagcomp_safety_ticks, latency_ticks + 2 );

		cache.now_tick = now_tick;
		cache.budget_ticks = std::max( cstypes::time_to_ticks( max_unlag ) - dynamic_safety, 2 );
		cache.valid = true;
	}

	bool shared::lagcomp::record::is_valid( ) const
	{
		if ( !this->valid )
		{
			return false;
		}

		const auto gen = lc_budget_generation( ).load( std::memory_order_relaxed );
		auto& cache = lc_budget_cache( );
		if ( cache.gen != gen )
		{
			compute_lc_budget( cache, gen );
		}
		if ( !cache.valid )
		{
			return false;
		}

		const auto age = cache.now_tick - this->tick;
		return age >= -1 && age <= cache.budget_ticks;
	}

	void shared::lagcomp::record::apply( )
	{
		if ( !this->valid || this->is_applied || !memory::is_game_ptr( this->game_scene_node ) )
		{
			return;
		}

		if ( this->bone_count <= 0 || this->bone_count > 128 )
		{
			return;
		}

		this->bone_cache = memory::read<std::uintptr_t>( this->game_scene_node + SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash ) + 0x80 );
		if ( !memory::is_game_ptr( this->bone_cache ) )
		{
			return;
		}

		const auto size = sizeof( systems::bones::data ) * this->bone_count;
		auto& scratch = applied_bone_scratch( );
		std::memcpy( scratch.data( ), reinterpret_cast< void* >( this->bone_cache ), size );
		std::memcpy( reinterpret_cast< void* >( this->bone_cache ), this->bones, size );

		this->is_applied = true;
	}

	void shared::lagcomp::record::restore( )
	{
		if ( !this->valid || !this->is_applied || !memory::is_game_ptr( this->bone_cache ) )
		{
			return;
		}

		if ( this->bone_count <= 0 || this->bone_count > 128 )
		{
			return;
		}

		const auto size = sizeof( systems::bones::data ) * this->bone_count;
		std::memcpy( reinterpret_cast< void* >( this->bone_cache ), applied_bone_scratch( ).data( ), size );

		this->is_applied = false;
	}

	void shared::lagcomp::clear( )
	{
		std::unique_lock records_lock( this->m_records_mtx );
		this->m_records.clear( );
		this->m_last_health.clear( );
		this->m_run_active.clear( );
		this->m_run_pending.clear( );
	}

	void shared::lagcomp::run( )
	{
		std::unique_lock records_lock( this->m_records_mtx );

		lc_budget_generation( ).fetch_add( 1, std::memory_order_relaxed );

		const auto local = systems::g_local.get( );
		if ( systems::g_lifecycle.busy( ) || !local.is_alive || !memory::is_game_ptr( local.pawn ) )
		{
			this->m_records.clear( );
			this->m_last_health.clear( );
			return;
		}

		auto& active = this->m_run_active;
		active.clear( );

		for ( const auto& p : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
			if ( !p.ptr || p.ptr == local.controller )
			{
				continue;
			}

			if ( !reinterpret_cast<CCSPlayerController*>(p.ptr)->m_bPawnIsAlive() )
			{
				continue;
			}

			const auto pawn = systems::g_entities.player_pawn( p.ptr );

			if ( !pawn || pawn == local.pawn )
			{
				continue;
			}

			const auto team = reinterpret_cast<C_BaseEntity*>(pawn)->m_iTeamNum();
			if ( !local.is_this_other_team( team ) )
			{
				continue;
			}

			active.insert( pawn );
		}

		std::erase_if( this->m_records, [ & ]( const auto& pair ) { return !active.contains( pair.first ); } );
		std::erase_if( this->m_last_health, [ & ]( const auto& pair ) { return !active.contains( pair.first ); } );

		auto& pending = this->m_run_pending;
		pending.clear( );
		pending.reserve( active.size( ) );

		for ( const auto& pawn : active )
		{
			const auto health = reinterpret_cast<C_BaseEntity*>(pawn)->m_iHealth();
			if ( health <= 0 )
			{
				this->m_records.erase( pawn );
				this->m_last_health[ pawn ] = 0;
				continue;
			}

			auto& records = this->m_records[ pawn ];
			const auto simulation_time = reinterpret_cast<C_BaseEntity*>(pawn)->m_flSimulationTime();
			const auto simulation_tick = cstypes::time_to_ticks( simulation_time );

			const auto last_it = this->m_last_health.find( pawn );
			const auto had_last = last_it != this->m_last_health.end( );
			const auto last_hp = had_last ? last_it->second : health;
			const auto respawned = had_last && last_hp <= 0 && health > 0;
			this->m_last_health[ pawn ] = health;

			const auto sim_rollback =
				!records.empty( ) && simulation_tick < ( records.front( ).tick - 1 );

			if ( respawned || sim_rollback )
			{
				records.clear( );
			}

			if ( records.empty( ) || simulation_tick > records.front( ).tick )
			{
				pending.push_back( { pawn, simulation_tick } );
			}

			while ( !records.empty( ) && !records.back( ).is_valid( ) )
			{
				records.pop_back( );
			}

			while ( records.size( ) > rage::k_max_lagcomp_records )
			{
				records.pop_back( );
			}
		}

		if ( pending.empty( ) )
		{
			return;
		}

		for ( auto& p : pending )
		{
			record rec{};

			if ( rec.setup( p.pawn ) )
			{
				auto& deque = this->m_records[ p.pawn ];
				if ( !deque.empty( ) )
				{
					const auto& front = deque.front( );
					if ( rec.tick == front.tick )
					{
						deque.front( ) = std::move( rec );
						continue;
					}

					const auto delta = rec.origin - front.origin;
					const auto dt_ticks = std::max( rec.tick - front.tick, 1 );
					const auto max_travel = 128.0f * static_cast< float >( dt_ticks );
					if ( delta.length_sqr( ) > max_travel * max_travel )
					{
						deque.clear( );
					}
				}

				deque.emplace_front( std::move( rec ) );
			}
		}

		for ( auto& [pawn, records] : this->m_records )
		{
			for ( auto& rec : records )
			{
				rec.was_valid = rec.is_valid( );
			}
		}
	}

	std::optional<shared::lagcomp::record> shared::lagcomp::get_oldest_valid( std::uintptr_t pawn ) const
	{
		std::shared_lock records_lock( this->m_records_mtx );

		const auto it = this->m_records.find( pawn );
		if ( it == this->m_records.end( ) || it->second.empty( ) )
		{
			return std::nullopt;
		}

		for ( auto rit = it->second.rbegin( ); rit != it->second.rend( ); ++rit )
		{
			if ( rit->is_valid( ) )
			{
				return *rit;
			}
		}

		return std::nullopt;
	}

	std::optional<shared::lagcomp::record> shared::lagcomp::get_oldest_was_valid( std::uintptr_t pawn ) const
	{
		std::shared_lock records_lock( this->m_records_mtx );

		const auto it = this->m_records.find( pawn );
		if ( it == this->m_records.end( ) || it->second.empty( ) )
		{
			return std::nullopt;
		}

		for ( auto rit = it->second.rbegin( ); rit != it->second.rend( ); ++rit )
		{
			if ( rit->was_valid )
			{
				return *rit;
			}
		}

		return std::nullopt;
	}

	std::optional<shared::lagcomp::visual_record> shared::lagcomp::get_oldest_was_valid_visual( std::uintptr_t pawn ) const
	{
		std::shared_lock records_lock( this->m_records_mtx );

		const auto it = this->m_records.find( pawn );
		if ( it == this->m_records.end( ) )
		{
			return std::nullopt;
		}

		for ( auto rit = it->second.rbegin( ); rit != it->second.rend( ); ++rit )
		{
			if ( !rit->was_valid )
			{
				continue;
			}

			visual_record out{};
			out.origin = rit->origin;
			for ( auto i = 0; i < 27; ++i )
			{
				out.bones[ i ] = rit->bones[ i ];
			}

			return out;
		}

		return std::nullopt;
	}

	std::vector<shared::lagcomp::record> shared::lagcomp::collect_valid_records( std::uintptr_t pawn, int max_ticks ) const
	{
		std::shared_lock records_lock( this->m_records_mtx );

		std::vector<record> result;

		const auto it = this->m_records.find( pawn );
		if ( it == this->m_records.end( ) || it->second.empty( ) )
		{
			return result;
		}

		int newest_tick{};
		bool has_newest{ false };
		for ( auto& rec : it->second )
		{
			if ( !rec.is_valid( ) )
			{
				continue;
			}
			newest_tick = rec.tick;
			has_newest = true;
			break;
		}

		if ( !has_newest )
		{
			return result;
		}

		result.reserve( it->second.size( ) );

		for ( auto& rec : it->second )
		{
			if ( !rec.is_valid( ) )
			{
				continue;
			}
			if ( max_ticks >= 0 && ( newest_tick - rec.tick ) > max_ticks )
			{
				continue;
			}
			result.push_back( rec );
		}

		return result;
	}

	std::vector<shared::lagcomp::record_tick_info> shared::lagcomp::collect_valid_tick_infos( std::uintptr_t pawn ) const
	{
		std::shared_lock records_lock( this->m_records_mtx );

		std::vector<record_tick_info> result;

		const auto it = this->m_records.find( pawn );
		if ( it == this->m_records.end( ) || it->second.empty( ) )
		{
			return result;
		}

		result.reserve( it->second.size( ) );

		for ( const auto& rec : it->second )
		{
			if ( !rec.is_valid( ) )
			{
				continue;
			}
			result.push_back( { rec.origin, rec.tick } );
		}

		return result;
	}

	std::vector<shared::lagcomp::record> shared::lagcomp::get_valid_records( std::uintptr_t pawn ) const
	{
		const auto max_ticks = std::clamp(
			settings::g_combat.m_lagcomp.max_backtrack_ticks.value,
			1,
			static_cast<int>( rage::k_max_lagcomp_records ) );
		return this->collect_valid_records( pawn, max_ticks );
	}

	std::vector<shared::lagcomp::record> shared::lagcomp::get_valid_records_for_extrapolation( std::uintptr_t pawn ) const
	{
		return this->collect_valid_records( pawn, -1 );
	}

	std::array<systems::bones::data, 27> shared::lagcomp::get_skeleton( const record& record ) const
	{
		std::array<systems::bones::data, 27> skeleton;

		if ( record.valid )
		{
			for ( auto i = 0; i < 27; ++i )
			{
				skeleton[ i ] = record.bones[ i ];
			}
		}

		return skeleton;
	}

}