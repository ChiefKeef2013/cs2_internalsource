#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/threadpool/threadpool.cpp>
#include <core/features.hpp>

namespace features::combat {

	rage::aim_context rage::build_context( systems::input::usercmd* cmd, const systems::local::snapshot& local ) const
	{
		auto& ctx = g_shared.ctx( );
		const auto& prestate = systems::g_prediction.pre( );

		aim_context out{};
		out.velocity = prestate.velocity;

		const auto simulated = systems::g_prediction.simulate( cmd, local, [ & ]
			{
				g_shared.sh( ).snapshot( local.pawn, ctx.weapon_services );

				out.velocity = memory::read<math::vector3>( local.pawn + SCHEMA( "C_BaseEntity", "m_vecAbsVelocity"_hash ) );
				out.spread = g_shared.get_spread( );
				out.predicted_inaccuracy = g_shared.get_inaccuracy( true );
			} );

		if ( !simulated )
		{
			out.spread = g_shared.get_spread( );
			out.predicted_inaccuracy = g_shared.get_inaccuracy( true );
		}

		ctx.spread = out.spread;
		ctx.inaccuracy = out.predicted_inaccuracy;

		out.view_angles = systems::g_input.get_view_angles( );
		out.on_ground = ( prestate.flags & cstypes::entity_flags::on_ground ) != 0;
		out.is_scoped = ctx.is_scoped;
		out.weapon_max_speed = ctx.weapon_max_speed;
		out.accurate_threshold = ctx.weapon_max_speed * 0.34f;

		return out;
	}

	std::optional<rage::stop_prediction> rage::predict_stop( const aim_context& ctx, const math::vector3& current_eye, const systems::local::snapshot& local ) const
	{
		const auto& shared_ctx = g_shared.ctx( );
		const auto& prestate = systems::g_prediction.pre( );
		const auto speed = prestate.networked_velocity.length_2d( );
		const auto will_stop = ctx.on_ground && ( speed > ctx.accurate_threshold || ( ctx.is_scoped && speed > 1.0f ) );

		if ( !will_stop )
		{
			return std::nullopt;
		}

		auto sim_vel = prestate.networked_velocity;
		sim_vel.z = 0.0f;

		const auto sv_friction = CONVAR("sv_friction")->get<float>( );
		const auto sv_stopspeed = CONVAR("sv_stopspeed")->get<float>( );
		const auto sv_accelerate = CONVAR("sv_accelerate")->get<float>( );
		const auto surface_friction = prestate.surface_friction;

		const auto movement_services = memory::read<std::uintptr_t>( local.pawn + SCHEMA( "C_BasePlayerPawn", "m_pMovementServices"_hash ) );
		const auto max_move_speed = movement_services ? memory::read<float>( movement_services + SCHEMA( "CPlayer_MovementServices", "m_flMaxspeed"_hash ) ) : 250.0f;

		auto stop_ticks = 0;
		auto counting = true;

		for ( auto i = 0; i < 15; ++i )
		{
			const auto sim_speed = sim_vel.length_2d( );

			if ( counting && sim_speed <= ctx.accurate_threshold )
			{
				counting = false;
			}

			if ( sim_speed < 1.0f )
			{
				break;
			}

			const auto control = std::fmaxf( sim_speed, sv_stopspeed );
			const auto drop = sv_friction * surface_friction * control * cstypes::tick_interval;
			auto new_speed = std::fmaxf( sim_speed - drop, 0.0f );
			auto accel = sv_accelerate;

			if ( shared_ctx.is_scoped )
			{
				const auto weapon_ratio = std::fminf( 1.0f, shared_ctx.weapon_max_speed / 250.0f );
				const auto scoped_max = std::fmaxf( 250.0f, max_move_speed ) * weapon_ratio * 0.52f;

				if ( new_speed > scoped_max - 5.0f )
				{
					const auto t = 1.0f - std::fmaxf( 0.0f, new_speed - ( scoped_max - 5.0f ) ) / std::fmaxf( 0.01f, 5.0f );
					accel *= std::clamp( t, 0.0f, 1.0f );
				}
			}

			const auto accel_speed = std::fminf( accel * shared_ctx.weapon_max_speed * surface_friction * cstypes::tick_interval, new_speed );
			new_speed = std::fmaxf( new_speed - accel_speed, 0.0f );

			if ( counting )
			{
				++stop_ticks;
			}

			if ( new_speed > 0.0f )
			{
				sim_vel *= ( new_speed / sim_speed );
			}
			else
			{
				sim_vel = {};
				break;
			}
		}

		const auto avg_vel = ( prestate.networked_velocity + sim_vel ) * 0.5f;
		const auto stop_time = static_cast< float >( stop_ticks ) * cstypes::tick_interval;

		return stop_prediction
		{
			.eye =
			{
				current_eye.x + avg_vel.x * stop_time,
				current_eye.y + avg_vel.y * stop_time,
				current_eye.z
			},
			.inaccuracy = g_shared.get_inaccuracy_at_velocity( local.pawn, sim_vel )
		};
	}

	std::vector<shared::lagcomp::record> rage::collect_candidate_records( std::uintptr_t pawn ) const
	{
		auto records = g_shared.lc( ).get_valid_records( pawn );
		if ( records.empty( ) )
		{
			return records;
		}

		const auto& newest = records.front( );
		auto moving = newest.velocity.length_2d( ) >= 1.0f;
		if ( !moving && records.size( ) >= 2 )
		{
			const auto step = newest.origin - records[ 1 ].origin;
			moving = ( step.x * step.x + step.y * step.y ) > 0.25f;
		}

		if ( !moving )
		{
			return records;
		}

		auto extrapolated = g_shared.lc( ).extrapolate( pawn );
		if ( !extrapolated.has_value( ) )
		{
			return records;
		}

		const auto delta = extrapolated->origin - records.front( ).origin;
		if ( delta.length_sqr( ) > 4.0f )
		{
			records.push_back( std::move( *extrapolated ) );
		}

		return records;
	}

	int rage::pick_candidate_record_indices(
		const std::vector<shared::lagcomp::record>& records,
		std::array<int, k_max_scan_records>& out_indices ) const
	{
		if ( records.empty( ) )
		{
			return 0;
		}

		const auto count = static_cast<int>( records.size( ) );
		const auto has_extrapolated = records.back( ).extrapolated;
		const auto historical_count = has_extrapolated ? count - 1 : count;

		auto picked = 0;
		const auto add_index = [&]( int idx )
		{
			if ( idx < 0 || idx >= count || picked >= k_max_scan_records )
			{
				return;
			}

			for ( auto i = 0; i < picked; ++i )
			{
				if ( out_indices[ i ] == idx )
				{
					return;
				}
			}

			out_indices[ picked++ ] = idx;
		};

		if ( historical_count <= 0 )
		{
			add_index( count - 1 );
			return picked;
		}

		const auto historical_budget =
			has_extrapolated
				? static_cast<int>( k_max_scan_records ) - 1
				: static_cast<int>( k_max_scan_records );

		add_index( 0 );
		add_index( historical_count - 1 );

		if ( historical_count > 2 && picked < historical_budget )
		{
			const auto interior_slots = historical_budget - picked;
			for ( auto i = 1; i <= interior_slots; ++i )
			{
				const auto idx =
					static_cast<int>(
						( static_cast<std::size_t>( i ) * ( historical_count - 1 ) ) /
						static_cast<std::size_t>( interior_slots + 1 ) );
				add_index( idx );
			}
		}

		if ( has_extrapolated && picked < k_max_scan_records )
		{
			add_index( count - 1 );
		}

		return picked;
	}

	std::vector<rage::candidate> rage::gather_candidates( const systems::local::snapshot& local, float max_distance_sq ) const
	{
		const auto& shared_ctx = g_shared.ctx( );
		const auto players = systems::g_entities.get_by_type( systems::entities::type::player );
		auto* self = const_cast<rage*>( this );

		std::vector<candidate> out;
		out.reserve( players.size( ) );

		self->m_candidate_records.clear( );
		self->m_candidate_records.reserve( players.size( ) * ( k_max_lagcomp_records + 1 ) );

		for ( const auto& p : players )
		{
			if ( !p.ptr || p.ptr == local.controller )
			{
				continue;
			}

			if ( !memory::read<bool>( p.ptr + SCHEMA( "CCSPlayerController", "m_bPawnIsAlive"_hash ) ) )
			{
				continue;
			}

			const auto pawn_handle = systems::g_entities.player_pawn_handle( p.ptr );
			const auto pawn = systems::g_entities.lookup( pawn_handle );

			if ( !pawn || pawn == local.pawn )
			{
				continue;
			}

			const auto team = memory::read<int>( pawn + SCHEMA( "C_BaseEntity", "m_iTeamNum"_hash ) );
			if ( !local.is_this_other_team( team ) )
			{
				continue;
			}

			const auto health = memory::read<int>( pawn + SCHEMA( "C_BaseEntity", "m_iHealth"_hash ) );
			if ( health <= 0 )
			{
				continue;
			}

			if ( memory::read<bool>( pawn + SCHEMA( "C_CSPlayerPawn", "m_bGunGameImmunity"_hash ) ) )
			{
				continue;
			}

			if ( max_distance_sq > 0.0f )
			{
				const auto tick_infos = g_shared.lc( ).collect_valid_tick_infos( pawn );
				if ( tick_infos.empty( ) )
				{
					continue;
				}

				const auto& origin = systems::g_prediction.pre( ).origin;
				auto closest_sq = std::numeric_limits<float>::max( );

#pragma loop( no_vector )
				for ( std::size_t i = 0; i < tick_infos.size( ); ++i )
				{
					const auto& info = tick_infos[ i ];
					const auto delta = info.origin - origin;
					const auto distance_sq =
						delta.x * delta.x +
						delta.y * delta.y +
						delta.z * delta.z;
					closest_sq = std::min( closest_sq, distance_sq );
				}

				if ( closest_sq > max_distance_sq )
				{
					continue;
				}
			}

			auto records = this->collect_candidate_records( pawn );
			if ( records.empty( ) )
			{
				continue;
			}

			const auto record_base = self->m_candidate_records.size( );
			self->m_candidate_records.insert(
				self->m_candidate_records.end( ),
				std::make_move_iterator( records.begin( ) ),
				std::make_move_iterator( records.end( ) ) );

			candidate c{};
			c.pawn = pawn;
			c.health = health;
			c.armor = memory::read<int>( pawn + SCHEMA( "C_CSPlayerPawn", "m_ArmorValue"_hash ) );

			std::array<int, k_max_scan_records> record_indices{};
			const auto picked_count = this->pick_candidate_record_indices( records, record_indices );

#pragma loop( no_vector )
			for ( auto i = 0; i < picked_count; ++i )
			{
				const auto record_index = static_cast< std::size_t >( record_indices[ i ] );
				c.records[ i ] = &self->m_candidate_records[ record_base + record_index ];
			}

			c.record_count = picked_count;

			c.all_record_count = static_cast< int >( std::min<std::size_t>( records.size( ), c.all_records.size( ) ) );
#pragma loop( no_vector )
			for ( auto i = 0; i < c.all_record_count; ++i )
			{
				c.all_records[ i ] = &self->m_candidate_records[ record_base + static_cast< std::size_t >( i ) ];
			}

			if ( shared_ctx.weapon_type >= cstypes::weapon_type::pistol && shared_ctx.weapon_type <= cstypes::weapon_type::lmg )
			{
				const auto& config = settings::g_combat.m_ragebot.get_group( shared_ctx.weapon_type, shared_ctx.item_def_idx );
				c.min_damage = this->get_min_damage( config, health, config.min_damage_override.value );
			}

			out.push_back( c );
		}

		return out;
	}

	float rage::get_standing_inaccuracy( const systems::local::snapshot& local, const aim_context& ctx ) const
	{
		const auto standing = g_shared.get_inaccuracy_at_velocity( local.pawn, {} );
		const auto& shared_ctx = g_shared.ctx( );
		if ( !shared_ctx.weapon_vdata )
		{
			return standing > 0.0f ? standing : ctx.predicted_inaccuracy;
		}

		const auto inaccuracy_stand = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyStand"_hash ) );
		return std::max( inaccuracy_stand, standing );
	}

	bool rage::should_stop_movement( const aim_context& ctx ) const
	{
		if ( !settings::g_combat.m_autos.stop.value )
		{
			return false;
		}

		const auto& shared_ctx = g_shared.ctx( );
		const auto& prestate = systems::g_prediction.pre( );
		const auto velocity = prestate.networked_velocity;

		if ( shared_ctx.weapon_type == cstypes::weapon_type::sniper && !ctx.is_scoped )
		{
			return false;
		}

		if ( ctx.on_ground )
		{
			const auto speed_2d = velocity.length_2d( );
			if ( speed_2d <= 0.1f )
			{
				return false;
			}

			const auto inaccuracy_move = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyMove"_hash ) );
			const auto inaccuracy_stand = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyStand"_hash ) );

			return speed_2d * inaccuracy_move > inaccuracy_stand;
		}

		if ( shared_ctx.weapon_type != cstypes::weapon_type::sniper )
		{
			return false;
		}

		if ( velocity.z > 140.0f )
		{
			return false;
		}

		const auto sv_gravity = CONVAR ("sv_gravity")->get<float>( );
		const auto sv_friction = CONVAR ("sv_friction")->get<float>( );
		const auto sv_stopspeed = CONVAR ("sv_stopspeed")->get<float>( );

		const auto inac_jump_initial = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyJumpInitial"_hash ) );
		const auto inac_jump_apex = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flInaccuracyJumpApex"_hash ) );
		const auto shootable_threshold = inac_jump_apex + 0.001f;
		const auto early_threshold = inac_jump_initial * 0.55f + inac_jump_apex * 0.45f;
		const auto air_inaccuracy = g_shared.get_air_inaccuracy( velocity.z, inac_jump_initial, inac_jump_apex );

		if ( air_inaccuracy <= shootable_threshold || air_inaccuracy <= early_threshold )
		{
			return true;
		}

		auto sim_vz = velocity.z;
		auto ticks_to_shootable{ 0 };

		for ( auto i = 1; i <= 32; ++i )
		{
			sim_vz -= sv_gravity * cstypes::tick_interval;

			if ( g_shared.get_air_inaccuracy( sim_vz, inac_jump_initial, inac_jump_apex ) <= shootable_threshold )
			{
				ticks_to_shootable = i;
				break;
			}
		}

		if ( ticks_to_shootable == 0 )
		{
			return false;
		}

		const auto speed_2d = velocity.length_2d( );
		const auto max_speed = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flMaxSpeed"_hash ) );
		const auto accurate_threshold = max_speed * 0.34f;

		if ( speed_2d <= accurate_threshold )
		{
			return true;
		}

		auto sim_speed = speed_2d;
		auto ticks_to_stop{ 32 };

		for ( auto i = 1; i <= 32; ++i )
		{
			const auto drop = std::fmaxf( sim_speed, sv_stopspeed ) * sv_friction * cstypes::tick_interval;
			sim_speed -= drop;

			if ( sim_speed <= accurate_threshold )
			{
				ticks_to_stop = i;
				break;
			}
		}

		return ticks_to_shootable <= ticks_to_stop + 2;
	}

}

#include <modules/rage/misc/prediction/systems_prediction.h>



