#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/threadpool/threadpool.cpp>
#include <core/features.hpp>

namespace features::combat {

	void rage::on_create_move( systems::input::usercmd* cmd )
	{
		this->clear_visualize_target( );

		auto& ctx = g_shared.ctx( );
		const auto local = systems::g_local.get( );

		if ( !ctx.valid )
		{
			this->m_revolver_cock_ticks = 0;
			this->m_pen_can_pen.store( false, std::memory_order_release );
			return;
		}

		this->sample_penetration_crosshair( local );

		this->m_should_stop = false;
		this->m_firing_this_tick = false;

		if ( !settings::g_combat.m_duckpeek.enabled.value )
		{
			this->m_release_duck_for_shot = false;
			this->m_duckpeek_reduck = false;
		}

		if ( this->m_zeus_fired )
		{
			this->m_zeus_fired = false;

			if ( settings::g_combat.m_zeusbot.drop_after && !systems::g_local.is_in_deathmatch( ) )
			{
				memory::call<void>(PATTERN(PATTERN_ENGINE_CLIENT_CMD), addresses::globals::source2engine_to_client, 0, "drop", 0x7ffef001 );
			}

			return;
		}

		const auto is_knife = ctx.weapon_type == cstypes::weapon_type::knife;
		const auto is_taser = ctx.weapon_type == cstypes::weapon_type::taser;

		if ( !is_knife && !is_taser && ( ctx.weapon_type < cstypes::weapon_type::pistol || ctx.weapon_type > cstypes::weapon_type::lmg ) )
		{
			return;
		}

		auto aim_ctx = this->build_context( cmd, local );

		if ( is_knife )
		{
			if ( !g_shared.can_shoot( cmd, local.controller ) )
			{
				return;
			}

			this->run_knife( cmd, aim_ctx, local );
		}
		else if ( is_taser )
		{
			if ( !g_shared.can_shoot( cmd, local.controller ) )
			{
				return;
			}

			this->run_taser( cmd, aim_ctx, local );
		}
		else if ( ctx.item_def_idx == cstypes::item_definition_index::weapon_r8_revolver )
		{
			this->auto_revolver( cmd, aim_ctx, local );
		}
		else
		{
			this->m_revolver_cock_ticks = 0;
			this->run_gun( cmd, aim_ctx, local, g_shared.can_shoot( cmd, local.controller ) );
		}
	}

	void rage::push_visualize( const scan_hit& hit )
	{
		this->update_visualize_target( hit );
	}

	void rage::clear_visualize_target( )
	{
		this->m_visualize = {};
	}

	void rage::update_visualize_target( const scan_hit& hit )
	{
		if ( !settings::g_combat.m_ragebot.visualize.value || !hit.record || !hit.record->valid )
		{
			return;
		}

		if ( hit.bone_index < 0 || hit.bone_index >= hit.record->bone_count )
		{
			return;
		}

		const auto& bone = hit.record->bones[ hit.bone_index ];
		if ( bone.position.length_sqr( ) < 1.0f )
		{
			return;
		}

		this->m_visualize.start = bone.rotation.rotate_vector( hit.hitbox.mins ) + bone.position;
		this->m_visualize.end = bone.rotation.rotate_vector( hit.hitbox.maxs ) + bone.position;
		this->m_visualize.radius = std::max( hit.hitbox.radius, 1.0f );
		this->m_visualize.active = true;
	}

	void rage::on_render( xdraw::draw_list& draw_list )
	{
		this->update_penetration_crosshair( systems::g_local.get( ) );
		this->draw_penetration_crosshair( draw_list );
		this->draw_visualize_aimbot( );
	}

	void rage::run_gun( systems::input::usercmd* cmd, const aim_context& ctx, const systems::local::snapshot& local, bool allow_fire )
	{
		if ( !settings::g_combat.m_ragebot.enabled )
		{
			return;
		}

		auto& shared_ctx = g_shared.ctx( );
		const auto& config = settings::g_combat.m_ragebot.get_group( shared_ctx.weapon_type, shared_ctx.item_def_idx );

		const selection_options opts{ config.prefer_safe_point.value, false };

		const auto finish_doubletap = [ & ]( bool charge_dt )
		{
			return charge_dt && this->process_doubletap( cmd, local, true );
		};

		auto candidates = this->gather_candidates( local );

		if ( candidates.empty( ) )
		{
			finish_doubletap( false );
			return;
		}

		auto eye_candidates = g_shared.sh( ).get_candidates( );
		if ( eye_candidates.count == 0 )
		{
			eye_candidates.entries[ 0 ].position = g_shared.get_shoot_position( );
			eye_candidates.entries[ 0 ].is_uninterpolated = true;
			eye_candidates.count = 1;
		}

		const auto scan_from_eye_candidates = [ & ]( const math::vector3& eye_offset, float inaccuracy )
		{
			std::vector<scan_hit> hits_out;

			for ( auto i = 0; i < eye_candidates.count; ++i )
			{
				const auto eye = eye_candidates.entries[ i ].position + eye_offset;
				auto hits = this->scan_players( eye, inaccuracy, ctx, candidates, local );

				for ( auto& hit : hits )
				{
					auto source_eye = eye_candidates.entries[ i ];
					source_eye.position = eye;
					hit.source_eye = source_eye;
					hits_out.push_back( std::move( hit ) );
				}
			}

			return hits_out;
		};

		if ( config.no_spread.value )
		{
			shared_ctx.inaccuracy = ctx.predicted_inaccuracy;
			auto all_hits = scan_from_eye_candidates( {}, shared_ctx.inaccuracy );

			if ( all_hits.empty( ) )
			{
				finish_doubletap( false );
				return;
			}

			const auto best = this->select_best( ctx, all_hits, shared_ctx.inaccuracy, candidates, local, opts );
			if ( !best.valid )
			{
				finish_doubletap( false );
				return;
			}

			this->update_visualize_target( best.hit );

			if ( !allow_fire )
			{
				return;
			}

			const auto subtick_attack = finish_doubletap( true );
			this->fire_gun( cmd, best, false, best.hit.source_eye.position, local, subtick_attack );
			return;
		}

		const auto primary_eye = eye_candidates.entries[ 0 ].position;
		const auto& prestate = systems::g_prediction.pre( );

		math::vector3 early_stop_offset{};
		auto scan_inaccuracy = ctx.predicted_inaccuracy;
		const auto moving_for_early_stop = ctx.on_ground &&
			prestate.networked_velocity.length_2d( ) > g_shared.ctx( ).weapon_max_speed * 0.34f &&
			this->should_stop_movement( ctx );

		if ( moving_for_early_stop )
		{
			if ( const auto stop = this->predict_stop( ctx, primary_eye, local ) )
			{
				early_stop_offset = stop->eye - primary_eye;
				scan_inaccuracy = stop->inaccuracy;
				this->m_should_stop = true;
			}
		}

		auto current_hits = scan_from_eye_candidates( early_stop_offset, scan_inaccuracy );
		auto best = this->select_best( ctx, current_hits, scan_inaccuracy, candidates, local, opts );

		const auto needed_hc = config.hitchance_override.value ? static_cast< float >( config.hitchance_override_value ) / 100.0f : static_cast< float >( config.hitchance ) / 100.0f;

		if ( config.auto_baim_on_fail.value && !config.body_aim.value && best.valid && best.hitchance < needed_hc )
		{
			selection_options body_opts = opts;
			body_opts.body_only = true;

			const auto body_best = this->select_best( ctx, current_hits, scan_inaccuracy, candidates, local, body_opts );
			if ( body_best.valid && body_best.hitchance >= needed_hc )
			{
				best = body_best;
			}
		}
		const auto duckpeek_active = settings::g_combat.m_duckpeek.enabled.value && ctx.on_ground;
		const auto is_ducked = ( prestate.flags & cstypes::entity_flags::ducking ) != 0;

		const auto standing_inaccuracy = duckpeek_active ? this->get_standing_inaccuracy( local, ctx ) : scan_inaccuracy;
		const auto standing_hc = best.valid
			? ( duckpeek_active ? this->evaluate_hitchance( best.hit, ctx, standing_inaccuracy ) : best.hitchance )
			: 0.0f;

		const auto accurate = best.valid && standing_hc >= needed_hc;
		const auto max_acc = g_shared.is_max_accuracy( standing_inaccuracy );
		const auto force_ground = best.valid && ctx.on_ground && config.force_shot.value && max_acc;
		const auto force = force_ground;
		const auto shot_viable = accurate || force;

		if ( !shot_viable && !this->m_should_stop && this->should_stop_movement( ctx ) )
		{
			const auto stop = this->predict_stop( ctx, primary_eye, local );
			if ( stop )
			{
				const auto future_offset = stop->eye - primary_eye;
				auto planned_hits = scan_from_eye_candidates( future_offset, stop->inaccuracy );
				const auto planned = this->select_best( ctx, planned_hits, stop->inaccuracy, candidates, local, opts );
				this->m_should_stop = planned.valid;
			}
			else
			{
				this->m_should_stop = best.valid;
			}
		}

		if ( !best.valid )
		{
			finish_doubletap( false );
			return;
		}

		this->update_visualize_target( best.hit );

		if ( allow_fire && this->try_auto_scope( cmd ) )
		{
			finish_doubletap( false );
			return;
		}

		if ( duckpeek_active && allow_fire )
		{
			if ( shot_viable )
			{
				this->m_release_duck_for_shot = true;
			}
			else if ( !this->m_duckpeek_reduck )
			{
				this->m_release_duck_for_shot = false;
			}
		}

		auto ready_to_fire = shot_viable;
		if ( duckpeek_active )
		{
			if ( is_ducked )
			{
				ready_to_fire = false;
			}
			else
			{
				ready_to_fire = ready_to_fire && this->m_release_duck_for_shot;
			}
		}

		if ( ready_to_fire && allow_fire )
		{
			const auto subtick_attack = finish_doubletap( true );
			this->fire_gun( cmd, best, !accurate && force, best.hit.source_eye.position, local, subtick_attack );

			if ( duckpeek_active )
			{
				this->m_duckpeek_reduck = true;
				this->m_release_duck_for_shot = false;
			}
		}
		else
		{
			finish_doubletap( false );
		}
	}

	void rage::run_taser( systems::input::usercmd* cmd, const aim_context& ctx, const systems::local::snapshot& local )
	{
		if ( !settings::g_combat.m_zeusbot.enabled )
		{
			return;
		}

		auto candidates = this->gather_candidates( local );
		if ( candidates.empty( ) )
		{
			return;
		}

		auto eye_candidates = g_shared.sh( ).get_candidates( );
		if ( eye_candidates.count == 0 )
		{
			eye_candidates.entries[ 0 ].position = g_shared.get_shoot_position( );
			eye_candidates.entries[ 0 ].is_uninterpolated = true;
			eye_candidates.count = 1;
		}

		std::vector<scan_hit> all_hits;

		for ( auto i = 0; i < eye_candidates.count; ++i )
		{
			auto hits = this->scan_taser( eye_candidates.entries[ i ].position, ctx, candidates, local );

			for ( auto& h : hits )
			{
				h.source_eye = eye_candidates.entries[ i ];
				all_hits.push_back( std::move( h ) );
			}
		}

		if ( all_hits.empty( ) )
		{
			return;
		}

		target best{};

		for ( const auto& h : all_hits )
		{
			if ( !best.valid || h.score > best.score )
			{
				best.hit = h;
				best.hitchance = 1.0f;
				best.score = h.score;
				best.valid = true;
			}
		}

		if ( best.valid )
		{
			this->m_zeus_fired = true;
			this->fire_melee( cmd, best, local );
		}
	}

	void rage::run_knife( systems::input::usercmd* cmd, const aim_context& ctx, const systems::local::snapshot& local )
	{
		if ( !settings::g_combat.m_knifebot.enabled )
		{
			return;
		}

		const auto info = this->get_knife_info( local );
		if ( !info.can_slash && !info.can_stab )
		{
			return;
		}

		constexpr auto max_knife_dist_sq = 150.0f * 150.0f;
		auto candidates = this->gather_candidates( local, max_knife_dist_sq );
		if ( candidates.empty( ) )
		{
			return;
		}

		auto eye_candidates = g_shared.sh( ).get_candidates( );
		if ( eye_candidates.count == 0 )
		{
			eye_candidates.entries[ 0 ].position = g_shared.get_shoot_position( );
			eye_candidates.entries[ 0 ].is_uninterpolated = true;
			eye_candidates.count = 1;
		}

		std::vector<scan_hit> all_hits;

		for ( auto i = 0; i < eye_candidates.count; ++i )
		{
			auto hits = this->scan_knife( eye_candidates.entries[ i ].position, ctx, info, candidates, local );

			for ( auto& h : hits )
			{
				h.source_eye = eye_candidates.entries[ i ];
				all_hits.push_back( std::move( h ) );
			}
		}

		if ( all_hits.empty( ) )
		{
			return;
		}

		target best{};
		target best_backstab{};

		for ( const auto& h : all_hits )
		{
			auto& dest = h.is_backstab ? best_backstab : best;

			if ( !dest.valid || h.score > dest.score )
			{
				dest.hit = h;
				dest.hitchance = 1.0f;
				dest.score = h.score;
				dest.valid = true;
			}
		}

		auto& chosen = best_backstab.valid ? best_backstab : best;
		if ( !chosen.valid )
		{
			return;
		}

		this->m_knife_attack = static_cast< std::uint8_t >( chosen.hit.attack_type );
		this->fire_melee( cmd, chosen, local );
	}

	std::vector<rage::scan_hit> rage::scan_players( const math::vector3& eye, float inaccuracy, const aim_context& ctx, std::vector<candidate>& candidates, const systems::local::snapshot& local ) const
	{
		std::vector<std::vector<scan_hit>> per_candidate( candidates.size( ) );

	threadpool::parallel_for( 0, static_cast< int >( candidates.size( ) ), [ & ]( int begin, int end )
			{
				for ( auto ci = begin; ci < end; ++ci )
				{
					auto& cand = candidates[ ci ];
					auto& candidate_hits = per_candidate[ ci ];
					candidate_hits.reserve( 24 );

					for ( auto ri = 0; ri < cand.record_count; ++ri )
					{
						if ( !cand.records[ ri ] || !cand.records[ ri ]->valid )
						{
							continue;
						}

						const auto& hits = this->scan_player( eye, inaccuracy, ctx, cand, cand.records[ ri ], local );

						for ( const auto& h : hits )
						{
							candidate_hits.push_back( h );
						}
					}
				}
			}, 1 );

		std::vector<scan_hit> flat;
		auto total_hits{ std::size_t{} };
		for ( const auto& hits : per_candidate )
		{
			total_hits += hits.size( );
		}
		flat.reserve( total_hits );

		for ( auto& v : per_candidate )
		{
			for ( auto& h : v )
			{
				flat.push_back( std::move( h ) );
			}
		}

		return flat;
	}

	const std::vector<rage::scan_hit>& rage::scan_player( const math::vector3& eye, float inaccuracy, const aim_context& ctx, candidate& cand, shared::lagcomp::record* record, const systems::local::snapshot& local ) const
	{

		thread_local std::vector<scan_hit> results;
		results.clear( );

		if (!cand.pawn || cand.record_count <= 0 || cand.health <= 0)
			return results;

		const auto& shared_ctx = g_shared.ctx( );
		const auto& config = settings::g_combat.m_ragebot.get_group( shared_ctx.weapon_type, shared_ctx.item_def_idx );

		const auto game_scene_node = memory::read<std::uintptr_t>( cand.pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
		const auto hitbox_set = systems::g_hitboxes.query( game_scene_node );
		const auto skeleton = g_shared.lc( ).get_skeleton( *record );
		const auto pen_ctx = g_shared.pen( ).prepare_target( cand.pawn, record );

		constexpr int k_max_hitbox_index = 32;
		const systems::hitboxes::entry* hitbox_by_index[ k_max_hitbox_index ]{};
		for ( const auto& entry : hitbox_set )
		{
			if ( entry.index >= 0 && entry.index < k_max_hitbox_index )
			{
				hitbox_by_index[ entry.index ] = &entry;
			}
		}

		const auto force_body = config.body_aim.value;

		std::array<int, 24> scan_order{};
		auto scan_count{ 0 };

		if ( !force_body && config.hitboxes.values[ 0 ] )
		{
			scan_order[ scan_count++ ] = 0;
		}

		if ( config.hitboxes.values[ 1 ] )
		{
			scan_order[ scan_count++ ] = 4;
			scan_order[ scan_count++ ] = 5;
			scan_order[ scan_count++ ] = 6;
		}

		if ( config.hitboxes.values[ 2 ] )
		{
			scan_order[ scan_count++ ] = 3;
			scan_order[ scan_count++ ] = 2;
		}

		if ( config.hitboxes.values[ 3 ] )
		{
			for ( auto idx : { 13, 14, 15, 16, 17, 18 } )
			{
				scan_order[ scan_count++ ] = idx;
			}
		}

		if ( config.hitboxes.values[ 4 ] )
		{
			for ( auto idx : { 7, 8, 9, 10 } )
			{
				scan_order[ scan_count++ ] = idx;
			}
		}

		if ( config.hitboxes.values[ 5 ] )
		{
			for ( auto idx : { 11, 12 } )
			{
				scan_order[ scan_count++ ] = idx;
			}
		}

		if ( config.hitboxes.values[ 6 ] )
		{
			for ( auto idx : { 19, 22 } )
			{
				scan_order[ scan_count++ ] = idx;
			}
		}

		if ( scan_count == 0 )
		{
			if ( !force_body )
			{
				scan_order[ scan_count++ ] = 0;
			}

			for ( auto idx : { 4, 5, 6, 3, 2 } )
			{
				scan_order[ scan_count++ ] = idx;
			}
		}

		struct trace_point
		{
			math::vector3 position;
			int hitbox_index;
			int bone_index;
			systems::hitboxes::entry hitbox;
			bool is_center;
		};

		thread_local std::vector<trace_point> points;
		points.clear( );
		points.reserve( static_cast< std::size_t >( scan_count ) * 12 );

		for ( auto idx = 0; idx < scan_count; ++idx )
		{
			const auto hitbox_index = scan_order[ idx ];
			const systems::hitboxes::entry* hb =
				( hitbox_index >= 0 && hitbox_index < k_max_hitbox_index )
					? hitbox_by_index[ hitbox_index ]
					: nullptr;

			if ( !hb || hb->bone < 0 || hb->bone >= 28 )
			{
				continue;
			}

			{
				const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hitbox_index );
				const auto max_dmg = g_shared.pen( ).get_max_damage( hitgroup, pen_ctx.target_armor, pen_ctx.has_helmet, pen_ctx.target_team );
				if ( max_dmg < cand.min_damage && max_dmg < static_cast< float >( cand.health ) )
				{
					continue;
				}
			}

			const auto& bone = skeleton[ hb->bone ];
			if ( bone.position.length_sqr( ) < 1.0f )
			{
				continue;
			}

			const auto hitbox_center = ( hb->mins + hb->maxs ) * 0.5f;
			const auto center = bone.rotation.rotate_vector( hitbox_center ) + bone.position;

			trace_point cp{};
			cp.position = center;
			cp.hitbox_index = hitbox_index;
			cp.bone_index = hb->bone;
			cp.hitbox = *hb;
			cp.is_center = true;
			points.push_back( cp );

			if ( config.pointscale > 0.0f )
			{
				const auto& mps = this->generate_multipoints( *hb, center, bone.rotation, config.pointscale, eye, inaccuracy );

				for ( const auto& mp : mps )
				{
					const auto duplicate = std::any_of( points.begin( ), points.end( ), [ & ]( const trace_point& point )
						{
							return point.hitbox_index == hitbox_index && ( point.position - mp ).length_sqr( ) < 0.01f;
						} );
					if ( duplicate )
					{
						continue;
					}

					trace_point tp{};
					tp.position = mp;
					tp.hitbox_index = hitbox_index;
					tp.bone_index = hb->bone;
					tp.hitbox = *hb;
					tp.is_center = false;
					points.push_back( tp );

				}
			}
		}

		if ( points.empty( ) )
		{
			return results;
		}

		results.reserve( static_cast< std::size_t >( scan_count ) * 2 );
		std::array<bool, 32> center_sufficient{};

		for ( const auto& tp : points )
		{
			if ( !tp.is_center && tp.hitbox_index >= 0 && tp.hitbox_index < static_cast< int >( center_sufficient.size( ) ) && center_sufficient[ tp.hitbox_index ] )
			{
				continue;
			}

			const auto aim = math::helpers::calculate_angle( eye, tp.position );
			const auto fov = math::helpers::angle_distance( ctx.view_angles, aim );

			shared::penetration::result pen{};
			if ( !g_shared.pen( ).run( eye, tp.position, pen_ctx, local.pawn, local.team, pen ) )
			{
				continue;
			}

			if ( pen.damage < cand.min_damage )
			{
				continue;
			}

			if ( !tp.is_center && tp.hitbox_index == 0 )
			{
				if ( pen.hitgroup != systems::g_hitboxes.hitgroup_from_hitbox( tp.hitbox_index ) )
				{
					continue;
				}
			}

			if ( tp.is_center && tp.hitbox_index >= 0 && tp.hitbox_index < static_cast< int >( center_sufficient.size( ) ) )
			{
				center_sufficient[ tp.hitbox_index ] = !pen.penetrated || pen.damage >= static_cast< float >( cand.health );
			}

			scan_hit h{};
			h.position = tp.position;
			h.aim_angle = aim;
			h.damage = pen.damage;
			h.fov = fov;
			h.hitbox_index = tp.hitbox_index;
			h.hitgroup = pen.hitgroup;
			h.bone_index = tp.bone_index;
			h.hitbox = tp.hitbox;
			h.is_center = tp.is_center;
			h.penetrated = pen.penetrated;
			h.pawn = cand.pawn;
			h.health = cand.health;
			h.record = record;

			results.push_back( h );
		}

		return results;
	}

	bool rage::verify_safe_point( const scan_hit& hit, const candidate& cand, const systems::local::snapshot& local, float threshold, float& out_min_damage ) const
	{
		out_min_damage = 0.0f;

		if ( cand.all_record_count <= 0 )
		{
			return false;
		}

		math::vector3 forward{};
	math::helpers::angle_vectors_left( hit.aim_angle, &forward );
		const auto end = hit.source_eye.position + forward * g_shared.ctx( ).range;

		auto min_damage = std::numeric_limits<float>::max( );

		auto pen_ctx = g_shared.pen( ).prepare_target( cand.pawn, nullptr );

		for ( auto i = 0; i < cand.all_record_count; ++i )
		{
			const auto* record = cand.all_records[ i ];
			if ( !record || !record->valid )
			{
				continue;
			}

			pen_ctx.record = record;
			if ( record->game_scene_node )
			{
				pen_ctx.hitboxes = systems::g_hitboxes.query( record->game_scene_node );
			}

			shared::penetration::result pen{};
			if ( !g_shared.pen( ).run( hit.source_eye.position, end, pen_ctx, local.pawn, local.team, pen ) || pen.damage < threshold )
			{
				return false;
			}

			min_damage = std::min( min_damage, pen.damage );
		}

		if ( min_damage == std::numeric_limits<float>::max( ) )
		{
			return false;
		}

		out_min_damage = min_damage;
		return true;
	}

	rage::target rage::select_best( const aim_context& aim_ctx, const std::vector<scan_hit>& hits, float eval_inaccuracy, const std::vector<candidate>& candidates, const systems::local::snapshot& local, const selection_options& options ) const
	{
		auto hitgroup_priority = [ ]( int hitbox_index ) -> int
			{
				if ( hitbox_index == 0 ) { return 4; }
				if ( hitbox_index >= 1 && hitbox_index <= 6 ) { return 3; }
				if ( hitbox_index >= 13 && hitbox_index <= 18 ) { return 2; }
				if ( hitbox_index >= 7 && hitbox_index <= 12 ) { return 1; }
				return 0;
			};

		auto is_body_hitbox = [ ]( int hitbox_index )
			{
				return hitbox_index == 2 || hitbox_index == 3;
			};

		constexpr auto top_k_per_record{ 8 };

		auto cheap_score = [ & ]( const scan_hit& h ) -> float
			{
				const auto lethal_bonus = h.damage >= static_cast< float >( h.health ) ? 100000.0f : 0.0f;
				const auto direct_bonus = h.penetrated ? 0.0f : 5000.0f;
				const auto center_bonus = h.is_center ? 2000.0f : 0.0f;

				return lethal_bonus + direct_bonus + center_bonus + h.damage * 20.0f +
					static_cast< float >( hitgroup_priority( h.hitbox_index ) ) * 10.0f - h.fov;
			};

		struct evaluated_hit
		{
			int hit_index;
			float hitchance;
			float score;
		};

		thread_local std::vector<int> order;
		thread_local std::vector<evaluated_hit> evaluated;
		order.clear( );
		evaluated.clear( );
		order.reserve( hits.size( ) );
		evaluated.reserve( hits.size( ) );

		auto newest_historical_tick = std::numeric_limits<int>::min( );
		auto newest_any_tick = std::numeric_limits<int>::min( );

		for ( auto i = 0; i < static_cast< int >( hits.size( ) ); ++i )
		{
			const auto& h = hits[ i ];

			if ( options.body_only && !is_body_hitbox( h.hitbox_index ) )
			{
				continue;
			}

			if ( !h.record || !h.record->valid )
			{
				continue;
			}

			order.push_back( i );
			newest_any_tick = std::max( newest_any_tick, h.record->tick );
			if ( !h.record->extrapolated )
			{
				newest_historical_tick = std::max( newest_historical_tick, h.record->tick );
			}
		}

		auto newest_tick = newest_historical_tick != std::numeric_limits<int>::min( )
			? newest_historical_tick
			: ( newest_any_tick != std::numeric_limits<int>::min( ) ? newest_any_tick : 0 );

		std::sort( order.begin( ), order.end( ), [ & ]( int a, int b )
			{
				if ( hits[ a ].record != hits[ b ].record )
				{
					return hits[ a ].record < hits[ b ].record;
				}

				const auto sa = cheap_score( hits[ a ] );
				const auto sb = cheap_score( hits[ b ] );
				if ( sa != sb )
				{
					return sa > sb;
				}
				return a < b;
			} );

		const auto& config = settings::g_combat.m_ragebot.get_group( g_shared.ctx( ).weapon_type, g_shared.ctx( ).item_def_idx );
		const auto needed_hc = config.hitchance_override.value
			? static_cast< float >( config.hitchance_override_value ) / 100.0f
			: static_cast< float >( config.hitchance ) / 100.0f;

		for ( auto run_begin = std::size_t{ 0 }; run_begin < order.size( ); )
		{
			const auto record = hits[ order[ run_begin ] ].record;

			auto run_end = run_begin;
			while ( run_end < order.size( ) && hits[ order[ run_end ] ].record == record )
			{
				++run_end;
			}

			const auto kept = std::min<std::size_t>( run_end - run_begin, top_k_per_record );

			for ( auto k = std::size_t{ 0 }; k < kept; ++k )
			{
				const auto idx = order[ run_begin + k ];
				const auto& h = hits[ idx ];

				if ( h.bone_index < 0 || h.bone_index >= 28 )
				{
					continue;
				}

				const auto& bone = record->bones[ h.bone_index ];
				const auto hc = config.no_spread.value
					? 1.0f
					: ( g_shared.is_max_accuracy( eval_inaccuracy )
						? 1.0f
						: g_shared.calculate_hitchance( h.source_eye.position, h.aim_angle, h.hitbox, bone, eval_inaccuracy, aim_ctx.spread ) );
				const auto hp = static_cast< float >( h.health );
				const auto can_kill = h.damage >= hp;
				const auto passes_hitchance = config.no_spread.value || hc >= needed_hc;
				auto score = passes_hitchance ? 1000000.0f : 0.0f;

				if ( can_kill )
				{
					score += 100000.0f + hc * 10000.0f;
				}
				else
				{
					score += h.damage * hc * 100.0f + h.damage * 5.0f;
				}

				score += h.penetrated ? 0.0f : 250.0f;
				score += h.is_center ? 50.0f : 0.0f;
				score += static_cast< float >( hitgroup_priority( h.hitbox_index ) ) * 2.0f;
				score -= h.fov * 0.1f;
				score -= record->extrapolated ? 5.0f : 0.0f;

				const auto age = std::max( 0, newest_tick - record->tick );
				score -= static_cast< float >( age ) * ( can_kill ? 30.0f : 120.0f );

				evaluated.push_back( evaluated_hit{ idx, hc, score } );
			}

			run_begin = run_end;
		}

		std::sort( evaluated.begin( ), evaluated.end( ), [ ]( const evaluated_hit& a, const evaluated_hit& b )
			{
				if ( a.score != b.score )
				{
					return a.score > b.score;
				}
				return a.hit_index < b.hit_index;
			} );

		target best{};
		target best_lethal_head{};
		target best_lethal_body{};

		auto consider = [ ]( target& slot, const scan_hit& h, float hc, float score )
			{
				if ( !slot.valid || score > slot.score )
				{
					slot.hit = h;
					slot.hitchance = hc;
					slot.score = score;
					slot.valid = true;
				}
			};

		for ( const auto& e : evaluated )
		{
			const auto& h = hits[ e.hit_index ];
			const auto is_head = h.hitbox_index == 0;
			const auto is_lethal = h.damage >= static_cast< float >( h.health );

			consider( best, h, e.hitchance, e.score );

			if ( is_lethal && is_head )
			{
				consider( best_lethal_head, h, e.hitchance, e.score );
			}
			else if ( is_lethal )
			{
				consider( best_lethal_body, h, e.hitchance, e.score );
			}
		}

		auto base_best = best_lethal_head.valid ? best_lethal_head : ( best_lethal_body.valid ? best_lethal_body : best );

		if ( !options.prefer_safe )
		{
			return base_best;
		}

		auto find_candidate = [ & ]( std::uintptr_t pawn ) -> const candidate*
			{
				for ( const auto& c : candidates )
				{
					if ( c.pawn == pawn )
					{
						return &c;
					}
				}
				return nullptr;
			};

		constexpr auto k_safe_verify_budget{ 16 };

		target safe_best{};
		target safe_lethal_best{};
		auto verified{ 0 };

		for ( const auto& e : evaluated )
		{
			if ( verified >= k_safe_verify_budget || safe_lethal_best.valid )
			{
				break;
			}

			const auto& h = hits[ e.hit_index ];

			if ( !config.no_spread.value && e.hitchance < needed_hc )
			{
				continue;
			}

			const auto cand = find_candidate( h.pawn );
			if ( !cand )
			{
				continue;
			}

			const auto threshold = std::max( cand->min_damage, 1.0f );

			float min_damage{};
			const auto safe = this->verify_safe_point( h, *cand, local, threshold, min_damage );
			++verified;

			if ( !safe )
			{
				continue;
			}

			auto safe_hit = h;
			safe_hit.is_safe = true;
			safe_hit.safe_min_damage = min_damage;

			consider( safe_best, safe_hit, e.hitchance, e.score );

			if ( min_damage >= static_cast< float >( h.health ) )
			{
				consider( safe_lethal_best, safe_hit, e.hitchance, e.score );
			}
		}

		if ( safe_lethal_best.valid )
		{
			return safe_lethal_best;
		}

		if ( base_best.valid && base_best.is_lethal( ) )
		{
			return base_best;
		}

		if ( safe_best.valid )
		{
			return safe_best;
		}

		return base_best;
	}

	std::vector<rage::scan_hit> rage::scan_taser( const math::vector3& eye, const aim_context& ctx, std::vector<candidate>& candidates, const systems::local::snapshot& local ) const
	{
		const auto& shared_ctx = g_shared.ctx( );
		std::vector<scan_hit> results;

		for ( auto& cand : candidates )
		{
			for ( auto ri = 0; ri < cand.record_count; ++ri )
			{
				auto* record = cand.records[ ri ];
				if ( !record || !record->valid )
				{
					continue;
				}

				const auto game_scene_node = memory::read<std::uintptr_t>( cand.pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
				if ( !game_scene_node )
				{
					continue;
				}

				const auto hitbox_set = systems::g_hitboxes.query( game_scene_node );
				if ( hitbox_set.count <= 0 )
				{
					continue;
				}

				record->apply( );
				const auto skeleton = g_shared.lc( ).get_skeleton( *record );

				for ( auto i = 0; i < hitbox_set.count; ++i )
				{
					const auto& hb = hitbox_set.entries[ i ];

					if ( hb.bone < 0 || hb.bone >= 28 )
					{
						continue;
					}

					const auto& bone = skeleton[ hb.bone ];
					if ( bone.position.length_sqr( ) < 1.0f )
					{
						continue;
					}

					const auto center = bone.rotation.rotate_vector( ( hb.mins + hb.maxs ) * 0.5f ) + bone.position;
					const auto aim = math::helpers::calculate_angle( eye, center );
					const auto fov = math::helpers::angle_distance( ctx.view_angles, aim );

					if ( fov > settings::g_combat.m_zeusbot.max_fov )
					{
						continue;
					}

					math::vector3 forward{};
				math::helpers::angle_vectors_left( aim, &forward );

					const auto trace = this->trace_taser_hit( eye, forward, shared_ctx.range * 0.85f, cand.pawn, local.pawn );
					if ( trace.hit_entity != cand.pawn )
					{
						continue;
					}

					const auto dist = ( center - eye ).length( );
					const auto range_fraction = dist / shared_ctx.range;

					scan_hit h{};
					h.position = center;
					h.aim_angle = aim;
					h.damage = 500.0f;
					h.score = ( 10000.0f - dist ) * ( range_fraction > 0.92f ? 0.8f : 1.0f );
					h.fov = fov;
					h.hitbox_index = hb.index;
					h.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
					h.bone_index = hb.bone;
					h.hitbox = hb;
					h.is_center = true;
					h.pawn = cand.pawn;
					h.health = cand.health;
					h.record = record;

					results.push_back( h );
				}

				record->restore( );
			}
		}

		return results;
	}

	rage::knife_info rage::get_knife_info( const systems::local::snapshot& local ) const
	{
		const auto& shared_ctx = g_shared.ctx( );
		const auto tick_base = memory::read<int>( local.controller + SCHEMA( "CBasePlayerController", "m_nTickBase"_hash ) );
		const auto next_primary = memory::read<int>( shared_ctx.weapon + SCHEMA( "C_BasePlayerWeapon", "m_nNextPrimaryAttackTick"_hash ) );
		const auto next_secondary = memory::read<int>( shared_ctx.weapon + SCHEMA( "C_BasePlayerWeapon", "m_nNextSecondaryAttackTick"_hash ) );
		const auto last_shot_time = memory::read<float>( shared_ctx.weapon + SCHEMA( "C_CSWeaponBase", "m_fLastShotTime"_hash ) );
		const auto cur_time = static_cast< float >( tick_base ) * cstypes::tick_interval;

		return knife_info
		{
			.can_slash = tick_base >= next_primary,
			.can_stab = tick_base >= next_secondary,
			.charged = ( cur_time - last_shot_time ) > 0.4f,
			.armor_ratio = memory::read<float>( shared_ctx.weapon_vdata + SCHEMA( "CCSWeaponBaseVData", "m_flArmorRatio"_hash ) )
		};
	}

	std::vector<rage::scan_hit> rage::scan_knife( const math::vector3& eye, const aim_context& ctx, const knife_info& info, std::vector<candidate>& candidates, const systems::local::snapshot& local ) const
	{
		constexpr auto stab_range{ 50.0f };
		constexpr auto slash_range{ 66.0f };

		std::vector<scan_hit> results;

		for ( auto& cand : candidates )
		{
			const auto eye_angles = memory::read<math::vector3>( cand.pawn + SCHEMA( "C_CSPlayerPawn", "m_angEyeAngles"_hash ) );
			const auto hp = static_cast< float >( cand.health );

			const auto frontal_slash_dmg = this->get_knife_damage( info.charged ? 40.0f : 25.0f, cand.armor, info.armor_ratio );
			const auto frontal_stab_dmg = this->get_knife_damage( 65.0f, cand.armor, info.armor_ratio );
			const auto frontal_can_kill = ( info.can_slash && frontal_slash_dmg >= hp ) || ( info.can_stab && frontal_stab_dmg >= hp );

			for ( auto ri = 0; ri < cand.record_count; ++ri )
			{
				auto* record = cand.records[ ri ];
				if ( !record || !record->valid )
				{
					continue;
				}

				const auto game_scene_node = memory::read<std::uintptr_t>( cand.pawn + SCHEMA( "C_BaseEntity", "m_pGameSceneNode"_hash ) );
				if ( !game_scene_node )
				{
					continue;
				}

				const auto hitbox_set = systems::g_hitboxes.query( game_scene_node );
				if ( hitbox_set.count <= 0 )
				{
					continue;
				}

				record->apply( );
				const auto skeleton = g_shared.lc( ).get_skeleton( *record );

				auto backstab{ false };
				{
					const auto delta = record->origin - systems::g_prediction.pre( ).origin;
					const auto dist_2d = std::sqrtf( delta.x * delta.x + delta.y * delta.y );

					if ( dist_2d > 0.001f )
					{
						const auto dir_x = delta.x / dist_2d;
						const auto dir_y = delta.y / dist_2d;

						math::vector3 body_forward{};
						math::helpers::angle_vectors_left( record->rotation, &body_forward );

						math::vector3 eye_forward{};
						math::helpers::angle_vectors_left( eye_angles, &eye_forward );

						backstab = ( dir_x * body_forward.x + dir_y * body_forward.y ) > 0.475f ||
							( dir_x * eye_forward.x + dir_y * eye_forward.y ) > 0.475f;
					}
				}

				const auto wait_for_backstab = backstab && !frontal_can_kill;

				for ( auto i = 0; i < hitbox_set.count; ++i )
				{
					const auto& hb = hitbox_set.entries[ i ];

					if ( hb.bone < 0 || hb.bone >= 28 )
					{
						continue;
					}

					const auto& bone = skeleton[ hb.bone ];
					if ( bone.position.length_sqr( ) < 1.0f )
					{
						continue;
					}

					const auto center = bone.rotation.rotate_vector( ( hb.mins + hb.maxs ) * 0.5f ) + bone.position;
					const auto dist = ( center - eye ).length( );
					const auto max_reach = info.can_slash ? slash_range : stab_range;

					if ( dist > max_reach )
					{
						continue;
					}

					const auto aim = math::helpers::calculate_angle( eye, center );
					const auto fov = math::helpers::angle_distance( ctx.view_angles, aim );

					if ( fov > settings::g_combat.m_knifebot.max_fov )
					{
						continue;
					}

					math::vector3 forward{};
					math::helpers::angle_vectors_left( aim, &forward );

					for ( const auto try_stab : { true, false } )
					{
						if ( try_stab && !info.can_stab )
						{
							continue;
						}

						if ( !try_stab && !info.can_slash )
						{
							continue;
						}

						const auto reach = try_stab ? stab_range : slash_range;
						if ( dist > reach )
						{
							continue;
						}

						const auto raw_dmg = try_stab ? ( backstab ? 180.0f : 65.0f ) : ( backstab ? 90.0f : ( info.charged ? 40.0f : 25.0f ) );
						const auto damage = this->get_knife_damage( raw_dmg, cand.armor, info.armor_ratio );
						const auto can_kill = damage >= hp;

						if ( wait_for_backstab && !can_kill )
						{
							continue;
						}

						const auto trace = this->trace_knife_hit( eye, forward, reach, cand.pawn, local.pawn );
						if ( trace.hit_entity != cand.pawn )
						{
							continue;
						}

						const auto reach_margin = 1.0f - ( dist / reach );

						scan_hit h{};
						h.position = center;
						h.aim_angle = aim;
						h.damage = damage;
						h.score = can_kill ? ( 10000.0f + damage * reach_margin ) : ( damage * 100.0f * reach_margin );
						h.fov = fov;
						h.hitbox_index = hb.index;
						h.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
						h.bone_index = hb.bone;
						h.hitbox = hb;
						h.is_center = true;
						h.is_backstab = backstab;
						h.attack_type = try_stab ? 1 : 0;
						h.pawn = cand.pawn;
						h.health = cand.health;
						h.record = record;

						results.push_back( h );
						break;
					}
				}

				record->restore( );
			}
		}

		return results;
	}

	systems::tracing::result rage::trace_taser_hit( const math::vector3& origin, const math::vector3& forward, float range, std::uintptr_t target_pawn, std::uintptr_t local_pawn ) const
	{
		const auto end = origin + forward * range;
		const int filter_extras[ ]{ 0, 15 };

		for ( const auto extra : filter_extras )
		{
			const auto filter = extra == 0 ? systems::g_tracing.make_filter( local_pawn, 0x001c1003, 4 ) : systems::g_tracing.make_filter( local_pawn, 0x001c1003, 4, 15 );
			auto result = systems::g_tracing.trace( origin, end, filter );

			if ( ( result.fraction < 1.0f || result.all_solid ) && result.hit_entity == target_pawn )
			{
				return result;
			}

			for ( auto radius = 2.0f; radius <= 4.0f; radius += 2.0f )
			{
				const auto sweep_end = end - forward * radius;
				result = systems::g_tracing.trace_sphere( origin, sweep_end, radius, filter );

				if ( ( result.fraction < 1.0f || result.all_solid ) && result.hit_entity == target_pawn )
				{
					return result;
				}
			}
		}

		systems::tracing::result miss{};
		miss.fraction = 1.0f;
		miss.hit_entity = 0;
		return miss;
	}

	systems::tracing::result rage::trace_knife_hit( const math::vector3& origin, const math::vector3& forward, float reach, std::uintptr_t target_pawn, std::uintptr_t local_pawn ) const
	{
		const auto end = origin + forward * reach;
		const auto knife_filter = systems::g_tracing.make_filter( local_pawn, 0x0c3001, 4 );
		auto result = systems::g_tracing.trace( origin, end, knife_filter );

		if ( ( result.fraction < 1.0f || result.all_solid ) && result.hit_entity == target_pawn )
		{
			return result;
		}

		const auto weapon_filter = systems::g_tracing.make_filter( local_pawn, 0x0c3001, 4, 15 );
		result = systems::g_tracing.trace( origin, end, weapon_filter );

		if ( ( result.fraction < 1.0f || result.all_solid ) && result.hit_entity == target_pawn )
		{
			return result;
		}

		for ( auto radius = 14.0f; radius > 0.0f; radius -= 3.0f )
		{
			const auto sweep_end = end - forward * radius;
			result = systems::g_tracing.trace_sphere( origin, sweep_end, radius, weapon_filter );

			if ( ( result.fraction < 1.0f || result.all_solid ) && result.hit_entity == target_pawn )
			{
				return result;
			}
		}

		result.fraction = 1.0f;
		result.hit_entity = 0;
		return result;
	}

}


