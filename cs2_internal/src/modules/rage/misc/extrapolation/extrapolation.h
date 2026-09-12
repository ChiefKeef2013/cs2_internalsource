#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>

namespace features::combat {

	void shared::lagcomp::predict_movement( extrapolation_data& data, std::uintptr_t skip_entity ) const
	{
		if ( !addresses::globals::game_trace_manager )
		{
			return;
		}

		const auto sv_gravity = CONVAR( "sv_gravity" )->get<float>( );
		const auto sv_friction = CONVAR( "sv_friction" )->get<float>( );
		const auto sv_stopspeed = CONVAR( "sv_stopspeed" )->get<float>( );
		const auto sv_standable_normal = CONVAR( "sv_standable_normal" )->get<float>( );

		if ( data.flags & cstypes::entity_flags::on_ground )
		{
			data.velocity.z = 0.0f;

			const auto speed = std::sqrtf( data.velocity.x * data.velocity.x + data.velocity.y * data.velocity.y );
			if ( speed > 0.1f )
			{
				const auto control = std::fmaxf( speed, sv_stopspeed );
				const auto drop = sv_friction * data.surface_friction * control * cstypes::tick_interval;
				const auto new_speed = std::fmaxf( speed - drop, 0.0f );
				const auto scale = new_speed / speed;
				data.velocity.x *= scale;
				data.velocity.y *= scale;
			}
			else
			{
				data.velocity.x = 0.0f;
				data.velocity.y = 0.0f;
			}
		}
		else
		{
			data.velocity.z -= sv_gravity * data.gravity_scale * cstypes::tick_interval;
		}

		const auto move_end = data.origin + data.velocity * cstypes::tick_interval;
		const auto bbox = systems::tracing::bbox_collision{ data.obb_mins, data.obb_maxs };
		const auto trace_mask = data.trace_mask ? data.trace_mask : std::uintptr_t{ 0x1c3003 };
		const auto movement_filter = systems::g_tracing.make_player_movement_filter( skip_entity, trace_mask, 11 );

		auto trace_result = systems::g_tracing.trace_player_bbox(
			data.origin,
			move_end,
			bbox,
			movement_filter,
			data.movement_services
		);

		if ( trace_result.fraction != 1.0f )
		{
			for ( auto i = 0; i < 2; ++i )
			{
				const auto dot = data.velocity.dot( trace_result.normal );
				data.velocity -= trace_result.normal * dot;

				const auto adjust = data.velocity.dot( trace_result.normal );
				if ( adjust < 0.0f )
				{
					data.velocity -= trace_result.normal * adjust;
				}

				const auto remaining_fraction = 1.0f - trace_result.fraction;
				const auto clip_end = trace_result.end_pos + data.velocity * ( cstypes::tick_interval * remaining_fraction );

				trace_result = systems::g_tracing.trace_player_bbox(
					trace_result.end_pos,
					clip_end,
					bbox,
					movement_filter,
					data.movement_services
				);

				if ( trace_result.fraction == 1.0f )
				{
					break;
				}
			}
		}

		data.origin = trace_result.end_pos;

		const auto ground_start = math::vector3{ data.origin.x, data.origin.y, data.origin.z + 2.0f };
		const auto ground_end = math::vector3{ data.origin.x, data.origin.y, data.origin.z - 4.0f };
		const auto ground_trace = systems::g_tracing.trace_player_bbox(
			ground_start,
			ground_end,
			bbox,
			movement_filter,
			data.movement_services
		);

		data.flags &= ~cstypes::entity_flags::on_ground;

		if ( ground_trace.fraction != 1.0f && ground_trace.normal.z >= sv_standable_normal )
		{
			data.flags |= cstypes::entity_flags::on_ground;
		}
	}

	std::optional<shared::lagcomp::record> shared::lagcomp::extrapolate( std::uintptr_t pawn )
	{
		if ( !settings::g_combat.m_lagcomp.extrapolation.value )
		{
			return std::nullopt;
		}

		const auto records = this->get_valid_records_for_extrapolation( pawn );
		if ( records.empty( ) )
		{
			return std::nullopt;
		}

		const auto& latest = records.front( );

		const auto net_client = addresses::globals::network_client_service;
		if ( !net_client )
		{
			return std::nullopt;
		}

		const auto tick_state = memory::call_vfunc<std::uintptr_t>( net_client, cstypes::offsets::net_tick_info_vfunc );
		if ( !tick_state )
		{
			return std::nullopt;
		}

		const auto server_tick = memory::read<int>( tick_state + cstypes::offsets::net_server_tick );
		const auto delta_ticks = server_tick - latest.tick;

		if ( delta_ticks < 0 )
		{
			return std::nullopt;
		}

		const auto max_extrap = settings::g_combat.m_lagcomp.max_extrapolate_ticks.value;
		if ( delta_ticks > max_extrap )
		{
			return std::nullopt;
		}

		auto ticks_to_extrapolate = delta_ticks + 1;
		if ( ticks_to_extrapolate > max_extrap )
		{
			ticks_to_extrapolate = max_extrap;
		}
		if ( ticks_to_extrapolate <= 0 )
		{
			return std::nullopt;
		}

		const auto on_ground_start = ( latest.flags & cstypes::entity_flags::on_ground ) != 0;
		if ( !on_ground_start )
		{
			ticks_to_extrapolate = std::min( ticks_to_extrapolate, 8 );
		}

		auto velocity = latest.velocity;
		float direction_change = 0.0f;
		bool velocity_uncertain = false;

		{
			math::vector3 velocity_sum{};
			auto weight_sum = 0.0f;
			const auto max_pairs = std::min<std::size_t>( records.size( ), 4 );

			for ( std::size_t i = 0; i + 1 < max_pairs; ++i )
			{
				const auto& newer = records[ i ];
				const auto& older = records[ i + 1 ];
				if ( !newer.valid || !older.valid )
				{
					break;
				}

				const auto dt = newer.simulation_time - older.simulation_time;
				if ( dt <= 0.0f )
				{
					continue;
				}

				const auto weight = 1.0f / static_cast< float >( i + 1 );
				velocity_sum += ( ( newer.origin - older.origin ) / dt ) * weight;
				weight_sum += weight;
			}

			if ( weight_sum > 0.0f )
			{
				const auto derived_velocity = velocity_sum / weight_sum;
				if ( derived_velocity.length_sqr( ) > 0.01f )
				{
					const auto latest_len_sqr = latest.velocity.x * latest.velocity.x + latest.velocity.y * latest.velocity.y;
					const auto derived_len_sqr = derived_velocity.x * derived_velocity.x + derived_velocity.y * derived_velocity.y;
					if ( latest_len_sqr > 100.0f && derived_len_sqr > 100.0f )
					{
						const auto dot = latest.velocity.x * derived_velocity.x + latest.velocity.y * derived_velocity.y;
						const auto denom = std::sqrtf( latest_len_sqr * derived_len_sqr );
						if ( denom > 0.0f )
						{
							const auto cos_angle = std::clamp( dot / denom, -1.0f, 1.0f );
							if ( cos_angle < 0.7071f )
							{
								velocity_uncertain = true;
							}
						}
					}

					velocity = derived_velocity;
				}
			}
		}

		int turn_samples_total = 0;
		int turn_samples_discarded = 0;

		{
			auto turn_sum = 0.0f;
			auto turn_weight = 0.0f;
			const auto usable = std::min<std::size_t>( records.size( ), 4 );

			for ( std::size_t i = 0; i + 2 < usable; ++i )
			{
				const auto& a = records[ i ];
				const auto& b = records[ i + 1 ];
				const auto& c = records[ i + 2 ];

				if ( !a.valid || !b.valid || !c.valid )
				{
					break;
				}

				const auto dt = a.simulation_time - b.simulation_time;
				if ( dt <= 0.0f )
				{
					continue;
				}

				const auto d0 = a.origin - b.origin;
				const auto d1 = b.origin - c.origin;

				if ( ( d0.x == 0.0f && d0.y == 0.0f ) || ( d1.x == 0.0f && d1.y == 0.0f ) )
				{
					continue;
				}

				const auto dir0 = std::atan2f( d0.y, d0.x ) * ( 180.0f / 3.14159265f );
				const auto dir1 = std::atan2f( d1.y, d1.x ) * ( 180.0f / 3.14159265f );

				auto angle_diff = dir0 - dir1;
				while ( angle_diff > 180.0f ) angle_diff -= 360.0f;
				while ( angle_diff < -180.0f ) angle_diff += 360.0f;

				++turn_samples_total;

				if ( std::fabsf( angle_diff ) > 60.0f )
				{
					++turn_samples_discarded;
					continue;
				}

				const auto weight = 1.0f / static_cast< float >( i + 1 );
				turn_sum += ( angle_diff / dt ) * cstypes::tick_interval * weight;
				turn_weight += weight;
			}

			if ( turn_weight > 0.0f )
			{
				direction_change = turn_sum / turn_weight;
			}
		}

		const auto speed = std::sqrtf( velocity.x * velocity.x + velocity.y * velocity.y );
		if ( speed < 0.1f )
		{
			return std::nullopt;
		}

		const auto per_tick_turn_cap = on_ground_start ? 4.0f : 3.0f;
		bool turn_clamped = false;
	if ( std::fabsf( direction_change ) > per_tick_turn_cap )
		{
			direction_change = std::copysignf( per_tick_turn_cap, direction_change );
			turn_clamped = true;
		}

		const bool turn_unstable =
			turn_samples_total >= 2 &&
			turn_samples_discarded * 2 >= turn_samples_total;

		if ( turn_clamped || turn_unstable || velocity_uncertain )
		{
			ticks_to_extrapolate = std::min( ticks_to_extrapolate, 4 );
		}

		const auto game_scene_node = memory::read<std::uintptr_t>( pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
		if ( !game_scene_node )
		{
			return std::nullopt;
		}

		const auto collision = memory::read<std::uintptr_t>( pawn + SCHEMA( "C_BaseEntity", "m_pCollision"_hash ) );
		math::vector3 obb_mins{}, obb_maxs{};

		if ( collision )
		{
			obb_mins = memory::read<math::vector3>( collision + SCHEMA( "CCollisionProperty", "m_vecMins"_hash ) );
			obb_maxs = memory::read<math::vector3>( collision + SCHEMA( "CCollisionProperty", "m_vecMaxs"_hash ) );
		}
		else
		{
			obb_mins = { -16.0f, -16.0f, 0.0f };
			obb_maxs = { 16.0f, 16.0f, 72.0f };
		}

		auto max_player_speed = 250.0f;
		const auto movement_services = reinterpret_cast<C_BasePlayerPawn*>( pawn )->m_pMovementServices();
		if ( movement_services )
		{
			const auto ms = reinterpret_cast<CPlayer_MovementServices*>( movement_services )->m_flMaxspeed();
			if ( ms > 0.0f )
			{
				max_player_speed = ms;
			}
		}

		auto trace_mask{ std::uintptr_t{ 0x1c3003 } };
		if ( movement_services )
		{
			trace_mask = memory::read<std::uintptr_t>( movement_services + 0xd48 );
		}

		extrapolation_data data{};
		data.origin = latest.origin;
		data.velocity = velocity;
		data.obb_mins = obb_mins;
		data.obb_maxs = obb_maxs;
		data.flags = latest.flags;
		data.sim_time = latest.simulation_time;
		data.surface_friction = latest.surface_friction > 0.0f ? latest.surface_friction : 1.0f;
		data.gravity_scale = latest.gravity_scale > 0.0f ? latest.gravity_scale : 1.0f;
		data.movement_services = movement_services;
		data.trace_mask = trace_mask;

		auto turn_deg = direction_change;
		auto total_turn = 0.0f;
		const auto k_max_total_turn = on_ground_start ? 45.0f : 30.0f;

		for ( auto i = 0; i < ticks_to_extrapolate; ++i )
		{
			const auto current_speed = std::sqrtf( data.velocity.x * data.velocity.x + data.velocity.y * data.velocity.y );

			if ( current_speed < 0.1f )
			{
				break;
			}

			if ( current_speed > max_player_speed )
			{
				const auto scale = max_player_speed / current_speed;
				data.velocity.x *= scale;
				data.velocity.y *= scale;
			}

		if ( turn_deg != 0.0f && std::fabsf( total_turn ) < k_max_total_turn )
			{
				auto step = turn_deg;
			if ( std::fabsf( total_turn + step ) > k_max_total_turn )
				{
					step = std::copysignf( k_max_total_turn - std::fabsf( total_turn ), turn_deg );
				}

				const auto rad = step * ( 3.14159265f / 180.0f );
				const auto turn_cos = std::cosf( rad );
				const auto turn_sin = std::sinf( rad );
				const auto turned_x = data.velocity.x * turn_cos - data.velocity.y * turn_sin;
				const auto turned_y = data.velocity.x * turn_sin + data.velocity.y * turn_cos;
				data.velocity.x = turned_x;
				data.velocity.y = turned_y;

				total_turn += step;
				turn_deg *= 0.92f;
			}

			data.sim_time += cstypes::tick_interval;

			this->predict_movement( data, pawn );
		}

		const auto origin_delta = data.origin - latest.origin;

		if ( origin_delta.length_sqr( ) < 0.01f )
		{
			return std::nullopt;
		}

		record extrap_record = latest;
		extrap_record.origin = data.origin;
		extrap_record.velocity = data.velocity;
		extrap_record.flags = data.flags;
		extrap_record.simulation_time = data.sim_time;
		extrap_record.surface_friction = data.surface_friction;
		extrap_record.gravity_scale = data.gravity_scale;
		extrap_record.tick = cstypes::time_to_ticks( data.sim_time );
		extrap_record.extrapolated = true;

		extrap_record.rotation.y += total_turn;
		while ( extrap_record.rotation.y > 180.0f ) extrap_record.rotation.y -= 360.0f;
		while ( extrap_record.rotation.y < -180.0f ) extrap_record.rotation.y += 360.0f;

		const auto turn_rad = total_turn * ( 3.14159265f / 180.0f );
		const auto turn_cos = std::cosf( turn_rad );
		const auto turn_sin = std::sinf( turn_rad );
		const auto pivot = latest.origin;

		for ( auto i = 0; i < extrap_record.bone_count && i < 128; ++i )
		{
			const auto rx = extrap_record.bones[ i ].position.x - pivot.x;
			const auto ry = extrap_record.bones[ i ].position.y - pivot.y;
			const auto rot_x = rx * turn_cos - ry * turn_sin;
			const auto rot_y = rx * turn_sin + ry * turn_cos;
			extrap_record.bones[ i ].position.x = pivot.x + rot_x + origin_delta.x;
			extrap_record.bones[ i ].position.y = pivot.y + rot_y + origin_delta.y;
			extrap_record.bones[ i ].position.z += origin_delta.z;
		}

		return extrap_record;
	}

}