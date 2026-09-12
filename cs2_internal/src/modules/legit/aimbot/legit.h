#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <valve/utils/random.cpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/common.hpp>
#include <modules/rage/misc/seed/seedtrigger.h>

namespace features::combat {

	void legit::on_create_move( systems::input::usercmd* cmd )
	{
		if ( !settings::g_combat.m_legitbot.enabled.value )
		{
			return;
		}

		if ( g_rage.is_firing_this_tick( ) )
		{
			return;
		}

		auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
		{
			return;
		}

		if ( ctx.weapon_type < cstypes::weapon_type::pistol || ctx.weapon_type > cstypes::weapon_type::lmg )
		{
			return;
		}

		const auto& config = settings::g_combat.m_legitbot.get_group( ctx.weapon_type, ctx.item_def_idx );
		const auto view_angles = systems::g_input.get_view_angles( );
		const auto local = systems::g_local.get( );
		const auto aim_punch = g_shared.get_aim_punch( local.pawn );

		this->m_cached_view_angles = view_angles;
		this->m_cached_aim_punch = aim_punch;

		this->m_target = {};

		const auto manual_attack = ( cmd->buttons.value & cstypes::command_buttons::in_attack ) != 0;

		if ( !g_shared.can_shoot( cmd, local.controller, false ) )
		{
			return;
		}

		auto shoot_position = g_shared.get_interpolated_shoot_position( local.pawn, false );
		ctx.spread = g_shared.get_spread( );
		ctx.inaccuracy = g_shared.get_inaccuracy( true );

		systems::g_prediction.simulate( cmd, local, [ & ]
			{
				shoot_position = g_shared.get_interpolated_shoot_position( local.pawn, false );

				ctx.spread = g_shared.get_spread( );
				ctx.inaccuracy = g_shared.get_inaccuracy( true );
			} );

		auto detection_angles = view_angles;
		if ( config.rcs.value && aim_punch.length_sqr( ) > 0.0001f )
		{
			detection_angles.x += aim_punch.x;
			detection_angles.y += aim_punch.y;
			math::helpers::normalize_angles( detection_angles );
		}

		if ( config.aimbot.value )
		{
			this->m_target = this->find_target( shoot_position, detection_angles, config, local );
			if ( this->m_target.has_target( ) )
			{
				this->apply_aimbot( cmd, this->m_target, view_angles, aim_punch, config, local );
			}
		}

		if ( !g_shared.can_shoot( cmd, local.controller ) )
		{
			return;
		}

		if ( settings::g_combat.m_legitbot.standalone_backtrack.value && manual_attack )
		{
			this->apply_standalone_backtrack( cmd, shoot_position, view_angles, local );
		}

		if ( config.triggerbot.value )
		{
			this->apply_triggerbot( cmd, shoot_position, view_angles, aim_punch, config, local );
		}
	}

	void legit::on_render( xdraw::draw_list& draw_list )
	{
		if ( !settings::g_combat.m_legitbot.enabled.value )
		{
			return;
		}

		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
		{
			return;
		}

		if ( ctx.weapon_type < cstypes::weapon_type::pistol || ctx.weapon_type > cstypes::weapon_type::lmg )
		{
			return;
		}

		const auto& config = settings::g_combat.m_legitbot.get_group( ctx.weapon_type, ctx.item_def_idx );

		if ( config.visualize_fov.value )
		{
			this->draw_fov( draw_list, this->m_cached_view_angles, this->m_cached_aim_punch, config.fov.value, config.fov_color, config.rcs.value );
		}
	}

	void legit::invalidate_if_needed( )
	{
		const auto local = systems::g_local.get( );
		if ( !local.is_alive || !local.pawn || !local.controller )
		{
			this->m_trigger_release_time = 0.0f;
			this->m_trigger_pending_pawn = 0;
			this->m_trigger_delay_start = 0.0f;
			this->m_last_local_pawn = 0;
			return;
		}

		if ( this->m_last_local_pawn && this->m_last_local_pawn != local.pawn )
		{
			this->m_trigger_release_time = 0.0f;
			this->m_trigger_pending_pawn = 0;
			this->m_trigger_delay_start = 0.0f;
		}
		this->m_last_local_pawn = local.pawn;
	}

	legit::target_result legit::find_target( const math::vector3& shoot_position, const math::vector3& view_angles, const settings::combat::legitbot::weapon_group& config, const systems::local::snapshot& local ) const
	{
		target_result best{};

		for ( const auto& p : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
			if ( !p.ptr || p.ptr == local.controller )
			{
				continue;
			}

			if ( !reinterpret_cast<CCSPlayerController*>( p.ptr )->m_bPawnIsAlive() )
			{
				continue;
			}

			const auto pawn = systems::g_entities.player_pawn( p.ptr );

			if ( !pawn || pawn == local.pawn )
			{
				continue;
			}

			const auto team = reinterpret_cast<C_BaseEntity*>( pawn )->m_iTeamNum();
			if ( !local.is_this_other_team( team ) )
			{
				continue;
			}

			const auto health = reinterpret_cast<C_BaseEntity*>( pawn )->m_iHealth();
			if ( health <= 0 )
			{
				continue;
			}

			if ( reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_bGunGameImmunity() )
			{
				continue;
			}

			const auto records = g_shared.lc( ).get_valid_records( pawn );
			if ( records.empty( ) )
			{
				continue;
			}

			const auto& record = records.front( );
			if ( !record.valid )
			{
				continue;
			}

			const auto point = this->scan_player( pawn, record, shoot_position, view_angles, config, local );
			if ( !point.valid )
			{
				continue;
			}

			const auto aim = math::helpers::calculate_angle( shoot_position, point.position );
			const auto fov = math::helpers::angle_distance( view_angles, aim );

			const auto can_kill = point.damage >= static_cast< float >( health );
			const auto best_can_kill = best.has_target( ) && best.best_point.damage >= static_cast< float >( best.health );

			auto score{ 1000.0f };

			if ( can_kill && !best_can_kill )
			{
				score += 10000.0f + point.damage;
			}
			else if ( can_kill == best_can_kill )
			{
				score += point.damage * 100.0f + ( 180.0f - fov );
			}
			else
			{
				continue;
			}

			if ( !best.has_target( ) || score > best.score )
			{
				best.pawn = pawn;
				best.aim_angle = aim;
				best.hitchance = 1.0f;
				best.score = score;
				best.fov = fov;
				best.health = health;
				best.best_point = point;
			}
		}

		return best;
	}

	legit::scan_point legit::scan_player( std::uintptr_t pawn, const shared::lagcomp::record& record, const math::vector3& shoot_position, const math::vector3& view_angles, const settings::combat::legitbot::weapon_group& config, const systems::local::snapshot& local ) const
	{
		struct hitbox_entry
		{
			std::size_t cfg_index;
			std::uint32_t bone_id;
			int hitgroup;
		};

		constexpr std::array<hitbox_entry, 7> hitbox_map
		{ {
			{ 0, cstypes::bone_ids::head,             1 },
			{ 1, cstypes::bone_ids::spine_3,          2 },
			{ 2, cstypes::bone_ids::spine_2,          3 },
			{ 3, cstypes::bone_ids::left_shoulder,    4 },
			{ 3, cstypes::bone_ids::right_shoulder,   5 },
			{ 4, cstypes::bone_ids::left_knee,        6 },
			{ 4, cstypes::bone_ids::right_knee,       7 },
		} };

		const auto pen_ctx = g_shared.pen( ).prepare_target( pawn, &record );
		const auto skeleton = g_shared.lc( ).get_skeleton( record );

		scan_point best{};
		best.fov = 999.0f;
		best.damage = -1.0f;

		for ( const auto& [cfg_idx, bone_id, hitgroup] : hitbox_map )
		{
			if ( !config.hitboxes.values[ cfg_idx ] )
			{
				continue;
			}

			const auto bone_index = static_cast< std::size_t >( bone_id );
			if ( bone_index >= 27 )
			{
				continue;
			}

			const auto& bone = skeleton[ bone_index ];
			if ( bone.position.length_sqr( ) < 1.0f )
			{
				continue;
			}

			const auto aim = math::helpers::calculate_angle( shoot_position, bone.position );
			const auto fov = math::helpers::angle_distance( view_angles, aim );

			if ( fov > config.fov.value )
			{
				continue;
			}

			shared::penetration::result pen{};
			if ( !g_shared.pen( ).run( shoot_position, bone.position, pen_ctx, local.pawn, local.team, pen ) )
			{
				continue;
			}

			const auto visible = !pen.penetrated;
			if ( !visible && !config.autowall.value )
			{
				continue;
			}

			if ( !visible && pen.damage < static_cast< float >( config.min_damage.value ) )
			{
				continue;
			}

			const auto is_better = ( pen.damage > best.damage ) || ( pen.damage == best.damage && fov < best.fov );
			if ( is_better )
			{
				best.position = bone.position;
				best.damage = pen.damage;
				best.fov = fov;
				best.hitgroup = pen.hitgroup;
				best.cfg_index = cfg_idx;
				best.bone_index = static_cast< int >( bone_index );
				best.visible = visible;
				best.is_center = true;
				best.valid = true;
			}
		}

		return best;
	}

	void legit::apply_aimbot( systems::input::usercmd* cmd, const target_result& tgt, const math::vector3& view_angles, const math::vector3& aim_punch, const settings::combat::legitbot::weapon_group& config, const systems::local::snapshot& local )
	{
		auto aim_angle = tgt.aim_angle;

		if ( config.rcs.value )
		{
			this->apply_rcs( aim_angle, aim_punch, config.rcs_min.value, config.rcs_max.value );
		}

		if ( config.smooth.value > 0 )
		{
			auto delta = aim_angle - view_angles;
		math::helpers::normalize_angles( delta );

			const auto delta_length = std::sqrtf( delta.x * delta.x + delta.y * delta.y );
			if ( delta_length < 0.001f )
			{
				this->m_remainder_x = 0.0f;
				this->m_remainder_y = 0.0f;
				return;
			}

			const auto base_smooth = static_cast< float >( config.smooth.value );
			const auto distance_factor = std::clamp( delta_length / 10.0f, 0.0f, 1.0f );
			const auto ease = 1.0f - std::powf( distance_factor, 2.0f );
			auto smooth_factor = ( 0.3f + ease * 0.7f ) / base_smooth;

			smooth_factor *= random::normal_clamped( 1.0f, 0.06f, 0.85f, 1.15f );

			const auto x_bias = random::normal_clamped( 1.0f, 0.02f, 0.95f, 1.05f );
			const auto y_bias = random::normal_clamped( 0.97f, 0.03f, 0.90f, 1.04f );

			auto move_x = delta.x * smooth_factor * x_bias;
			auto move_y = delta.y * smooth_factor * y_bias;

			if ( delta_length < 2.0f && delta_length > 0.3f && random::floating( 0.0f, 1.0f ) < 0.15f )
			{
				const auto overshoot = random::normal_clamped( 1.2f, 0.08f, 1.05f, 1.4f );
				move_x *= overshoot;
				move_y *= overshoot;
			}

			aim_angle = view_angles + math::vector3{ move_x, move_y, 0.0f };
			math::helpers::normalize_angles( aim_angle );
		}

		auto want_x = aim_angle.x - view_angles.x;
		auto want_y = aim_angle.y - view_angles.y;

		want_x += this->m_remainder_x;
		want_y += this->m_remainder_y;

		const auto sensitivity = CONVAR ("sensitivity")->get<float>( );
		const auto fov_adjust = reinterpret_cast<C_BasePlayerPawn*>( local.pawn )->m_flFOVSensitivityAdjust();
		const auto deg_per_count = sensitivity * 0.022f * fov_adjust;

		const auto counts_x = std::roundf( want_x / deg_per_count );
		const auto counts_y = std::roundf( want_y / deg_per_count );

		this->m_remainder_x = want_x - counts_x * deg_per_count;
		this->m_remainder_y = want_y - counts_y * deg_per_count;

		systems::g_legit_input.add_mouse_delta( counts_x * deg_per_count, counts_y * deg_per_count );
	}

	void legit::apply_standalone_backtrack( systems::input::usercmd* cmd, const math::vector3& shoot_position, const math::vector3& view_angles, const systems::local::snapshot& local )
	{
		const auto& ctx = g_shared.ctx( );
		if ( !ctx.valid )
		{
			return;
		}

		math::vector3 forward{};
		math::helpers::angle_vectors_left( view_angles, &forward );

		const auto range = ctx.range > 1.0f ? ctx.range : 8192.0f;
		const auto ray_delta = forward * range;

		std::optional<shared::lagcomp::record> best{};
		auto best_fraction{ 1.0f };

		for ( const auto& p : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
			if ( !p.ptr || p.ptr == local.controller )
			{
				continue;
			}

			if ( !reinterpret_cast<CCSPlayerController*>( p.ptr )->m_bPawnIsAlive( ) )
			{
				continue;
			}

			const auto pawn = systems::g_entities.player_pawn( p.ptr );
			if ( !pawn || pawn == local.pawn )
			{
				continue;
			}

			const auto team = reinterpret_cast<C_BaseEntity*>( pawn )->m_iTeamNum( );
			if ( !local.is_this_other_team( team ) )
			{
				continue;
			}

			if ( reinterpret_cast<C_BaseEntity*>( pawn )->m_iHealth( ) <= 0 )
			{
				continue;
			}

			if ( reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_bGunGameImmunity( ) )
			{
				continue;
			}

			const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( pawn )->m_pGameSceneNode( );
			if ( !game_scene_node )
			{
				continue;
			}

			const auto hitbox_set = systems::g_hitboxes.query( game_scene_node );
			if ( hitbox_set.count <= 0 )
			{
				continue;
			}

			const auto records = g_shared.lc( ).get_valid_records( pawn );
			if ( records.empty( ) )
			{
				continue;
			}

			for ( auto i = static_cast< int >( records.size( ) ) - 1; i >= 0; --i )
			{
				const auto& rec = records[ static_cast< std::size_t >( i ) ];
				if ( !rec.valid || rec.extrapolated || rec.bone_count <= 0 )
				{
					continue;
				}

				auto hit_fraction{ 1.0f };
				auto hit{ false };
				for ( const auto& hb : hitbox_set )
				{
					if ( hb.bone < 0 || hb.bone >= rec.bone_count )
					{
						continue;
					}

					const auto& bone = rec.bones[ hb.bone ];
					if ( bone.position.length_sqr( ) < 1.0f )
					{
						continue;
					}

					const auto capsule_start = bone.rotation.rotate_vector( hb.mins ) + bone.position;
					const auto capsule_end = bone.rotation.rotate_vector( hb.maxs ) + bone.position;
					const auto radius = hb.radius > 0.001f ? hb.radius : 1.8f;

					auto fraction{ 1.0f };
					if ( !g_shared.ray_vs_capsule( shoot_position, ray_delta, capsule_start, capsule_end, radius, fraction ) )
					{
						continue;
					}

					if ( !hit || fraction < hit_fraction )
					{
						hit_fraction = fraction;
						hit = true;
					}
				}

				if ( !hit )
				{
					continue;
				}

				const auto closer = hit_fraction + 0.001f < best_fraction;
				const auto same_aim = std::fabs( hit_fraction - best_fraction ) <= 0.001f;
				if ( !best.has_value( ) || closer || ( same_aim && rec.tick < best->tick ) )
				{
					best = rec;
					best_fraction = hit_fraction;
				}

				break;
			}
		}

		if ( !best.has_value( ) )
		{
			return;
		}

		const auto interp_ticks = ( std::max )( 1, g_shared.sh( ).lerp_ticks_int( ) + ( g_shared.sh( ).lerp_ticks_frac( ) > 0.5f ? 1 : 0 ) );
		const auto history_size = cmd->csgo_user_cmd.input_history_size( );
		for ( auto i = 0; i < history_size; ++i )
		{
			const auto entry = cmd->csgo_user_cmd.mutable_input_history( i );
			if ( !entry )
			{
				continue;
			}

			entry->set_render_tick_count( best->tick + interp_ticks );
			entry->set_render_tick_fraction( 0.0f );

			if ( entry->has_sv_interp0( ) )
			{
				const auto interp = entry->mutable_sv_interp0( );
				interp->set_src_tick( -1 );
				interp->set_dst_tick( -1 );
				interp->set_frac( 0.0f );
			}

			if ( entry->has_sv_interp1( ) )
			{
				const auto interp = entry->mutable_sv_interp1( );
				interp->set_src_tick( -1 );
				interp->set_dst_tick( -1 );
				interp->set_frac( 0.0f );
			}

			if ( entry->has_cl_interp( ) )
			{
				entry->mutable_cl_interp( )->set_frac( 0.0f );
			}
		}
	}

	void legit::apply_triggerbot( systems::input::usercmd* cmd, const math::vector3& shoot_position, const math::vector3& view_angles, const math::vector3& aim_punch, const settings::combat::legitbot::weapon_group& config, const systems::local::snapshot& local )
	{
		const auto& ctx = g_shared.ctx( );
		const auto mode = config.resolved_trigger_mode( );
		const auto seed_mode = mode == settings::combat::legitbot::trigger_mode::seed;
		const auto nospread_mode = mode == settings::combat::legitbot::trigger_mode::nospread;

		if ( this->m_trigger_release_time > ctx.current_time + 5.0f )
		{
			this->m_trigger_release_time = 0.0f;
			this->m_trigger_pending_pawn = 0;
			this->m_trigger_delay_start = 0.0f;
		}

		if ( this->m_trigger_release_time > 0.0f )
		{
			if ( ctx.current_time >= this->m_trigger_release_time )
			{
				this->m_trigger_release_time = 0.0f;
				this->m_trigger_pending_pawn = 0;
				return;
			}

			cmd->buttons.value |= cstypes::command_buttons::in_attack;
			cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
			return;
		}

		if ( this->m_trigger_delay_start > ctx.current_time + 5.0f || this->m_trigger_delay_start < 0.0f )
		{
			this->m_trigger_delay_start = 0.0f;
			this->m_trigger_pending_pawn = 0;
		}

		const auto corrected_angles = view_angles;

		math::vector3 bullet_dir{};
		auto seed_tick = reinterpret_cast<CBasePlayerController*>( local.controller )->m_nTickBase( );
		auto seed_frac{ 0.0f };
		if ( seed_mode )
		{
			const auto weapon = ctx.weapon;
			if ( weapon && !seed_trigger::seed_cycle_allows_fire( weapon, local.pawn ) )
			{
				return;
			}

			auto atk_idx = cmd->csgo_user_cmd.attack1_start_history_index( );
			atk_idx = seed_trigger::prefer_tip_hist_index( cmd, atk_idx, &view_angles );
			const auto hist_count = cmd->csgo_user_cmd.input_history_size( );
			if ( atk_idx >= 0 && atk_idx < hist_count )
			{
				if ( const auto* e = cmd->csgo_user_cmd.mutable_input_history( atk_idx ) )
				{
					if ( e->player_tick_count( ) > 0 )
					{
						seed_tick = e->player_tick_count( );
						seed_frac = e->player_tick_fraction( );
						if ( !std::isfinite( seed_frac ) )
						{
							seed_frac = 0.0f;
						}
					}
				}
			}
			if ( seed_tick <= 0 )
			{
				return;
			}

			const auto seed = g_shared.get_spread_seed( corrected_angles, seed_tick );
			const auto spread = g_shared.calculate_spread( static_cast<int>( seed ), ctx.inaccuracy, ctx.spread, ctx.recoil_index, ctx.item_def_idx, ctx.num_bullets );

			math::vector3 forward{}, left{}, up{};
			math::helpers::angle_vectors_left( corrected_angles, &forward, &left, &up );
			bullet_dir = ( forward + left * spread.x + up * spread.y ).normalized( );
		}
		else if ( nospread_mode )
		{
			math::helpers::angle_vectors_left( corrected_angles, &bullet_dir );
		}

		auto hit_pawn{ 0ull };
		math::vector3 hit_position{};
		shared::penetration::result pen{};
		std::optional<shared::lagcomp::record> hit_record{};
		auto found{ false };

		struct gathered_records
		{
			std::array<shared::lagcomp::record, 3> entries{};
			int count{};
		};

		const auto gather_records = [ & ]( std::uintptr_t pawn ) -> gathered_records
			{
				gathered_records out{};

				const auto records = g_shared.lc( ).get_valid_records( pawn );
				if ( records.empty( ) || !records.front( ).valid )
				{
					return out;
				}

				const auto add_index = [ & ]( std::size_t index )
					{
						if ( out.count >= static_cast< int >( out.entries.size( ) ) || index >= records.size( ) )
						{
							return;
						}

						const auto& rec = records[ index ];
						if ( !rec.valid )
						{
							return;
						}

						for ( auto i = 0; i < out.count; ++i )
						{
							const auto& existing = out.entries[ i ];
							if ( existing.tick == rec.tick && existing.simulation_time == rec.simulation_time )
							{
								return;
							}
						}

						out.entries[ out.count++ ] = rec;
					};

				add_index( 0 );

				if ( records.size( ) > 2 )
				{
					add_index( records.size( ) / 2 );
					add_index( records.size( ) - 1 );
				}
				else if ( records.size( ) > 1 )
				{
					add_index( records.size( ) - 1 );
				}

				return out;
			};

		const auto range = ctx.range > 1.0f ? ctx.range : 8192.0f;

		for ( const auto& p : systems::g_entities.get_by_type( systems::entities::type::player ) )
		{
			if ( !p.ptr || p.ptr == local.controller )
			{
				continue;
			}

			if ( !reinterpret_cast<CCSPlayerController*>( p.ptr )->m_bPawnIsAlive( ) )
			{
				continue;
			}

			const auto pawn = systems::g_entities.player_pawn( p.ptr );

			if ( !pawn || pawn == local.pawn )
			{
				continue;
			}

			const auto team = reinterpret_cast<C_BaseEntity*>( pawn )->m_iTeamNum( );
			if ( !local.is_this_other_team( team ) )
			{
				continue;
			}

			const auto health = reinterpret_cast<C_BaseEntity*>( pawn )->m_iHealth( );
			if ( health <= 0 )
			{
				continue;
			}

			if ( reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_bGunGameImmunity( ) )
			{
				continue;
			}

			const auto recs = gather_records( pawn );
			if ( recs.count <= 0 )
			{
				continue;
			}

			const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( pawn )->m_pGameSceneNode( );
			const auto hitbox_set = systems::g_hitboxes.query( game_scene_node );

			for ( auto i = 0; i < recs.count; ++i )
			{
				const auto& rec = recs.entries[ i ];
				const auto skeleton = g_shared.lc( ).get_skeleton( rec );

				if ( seed_mode || nospread_mode )
				{
					for ( const auto& hb : hitbox_set )
					{
						if ( hb.bone < 0 || hb.bone >= 28 )
						{
							continue;
						}

						const auto& bone = skeleton[ hb.bone ];
						if ( bone.position.length_sqr( ) < 1.0f )
						{
							continue;
						}

						const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( hb.index );
						if ( config.trigger_head_only.value && hitgroup != 1 )
						{
							continue;
						}

						const auto capsule_start = bone.rotation.rotate_vector( hb.mins ) + bone.position;
						const auto capsule_end = bone.rotation.rotate_vector( hb.maxs ) + bone.position;
						const auto radius = hb.radius > 0.0f ? hb.radius * 0.9f : 1.8f;

						auto fraction{ 1.0f };
						if ( !g_shared.ray_vs_capsule( shoot_position, bullet_dir * range, capsule_start, capsule_end, radius, fraction ) )
						{
							continue;
						}

						const auto hit_point = shoot_position + bullet_dir * range * fraction;
						const auto pen_ctx = g_shared.pen( ).prepare_target( pawn, &rec );

						shared::penetration::result candidate_pen{};
						if ( !g_shared.pen( ).run( shoot_position, hit_point, pen_ctx, local.pawn, local.team, candidate_pen ) )
						{
							continue;
						}

						if ( candidate_pen.penetrated )
						{
							if ( !config.autowall.value )
							{
								continue;
							}
							if ( candidate_pen.damage < static_cast< float >( config.min_damage.value ) )
							{
								continue;
							}
						}

						if ( !found || candidate_pen.damage > pen.damage )
						{
							hit_pawn = pawn;
							hit_position = hit_point;
							hit_record = rec;
							pen = candidate_pen;
							found = true;
						}

						break;
					}
				}
				else
				{
					const systems::hitboxes::entry* closest_hb{ nullptr };
					auto closest_fov{ FLT_MAX };

					for ( const auto& entry : hitbox_set )
					{
						if ( entry.bone < 0 || entry.bone >= 28 )
						{
							continue;
						}

						const auto& bone = skeleton[ entry.bone ];
						if ( bone.position.length_sqr( ) < 1.0f )
						{
							continue;
						}

						const auto hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( entry.index );
						if ( config.trigger_head_only.value && hitgroup != 1 )
						{
							continue;
						}

						const auto center = bone.rotation.rotate_vector( ( entry.mins + entry.maxs ) * 0.5f ) + bone.position;
						const auto aim = math::helpers::calculate_angle( shoot_position, center );
						const auto fov = math::helpers::angle_distance( corrected_angles, aim );

						const auto dist = ( center - shoot_position ).length( );
						const auto hitbox_radius = std::max( entry.radius, 1.0f );
						const auto max_fov = dist > 1.0f
							? std::clamp( std::atan2f( hitbox_radius, dist ) * 57.2957795f, 0.3f, 2.0f )
							: 2.0f;

						if ( fov > max_fov )
						{
							continue;
						}

						if ( fov < closest_fov )
						{
							closest_fov = fov;
							closest_hb = &entry;
						}
					}

					if ( !closest_hb )
					{
						continue;
					}

					const auto& bone = skeleton[ closest_hb->bone ];
					const auto target_point = bone.rotation.rotate_vector( ( closest_hb->mins + closest_hb->maxs ) * 0.5f ) + bone.position;
					const auto pen_ctx = g_shared.pen( ).prepare_target( pawn, &rec );

					shared::penetration::result candidate_pen{};
					if ( !g_shared.pen( ).run( shoot_position, target_point, pen_ctx, local.pawn, local.team, candidate_pen ) )
					{
						continue;
					}

					const auto visible = !candidate_pen.penetrated;
					if ( !visible )
					{
						if ( !config.autowall.value )
						{
							continue;
						}

						if ( candidate_pen.damage < static_cast< float >( config.min_damage.value ) )
						{
							continue;
						}
					}

					if ( !found || candidate_pen.damage > pen.damage )
					{
						hit_pawn = pawn;
						hit_position = target_point;
						hit_record = rec;
						pen = candidate_pen;
						found = true;
					}
				}
			}
		}

		if ( !found )
		{
			this->m_trigger_pending_pawn = 0;
			return;
		}

		if ( config.trigger_head_only.value && pen.hitgroup != 1 )
		{
			this->m_trigger_pending_pawn = 0;
			return;
		}

		if ( pen.penetrated && pen.damage < static_cast< float >( config.min_damage.value ) )
		{
			this->m_trigger_pending_pawn = 0;
			return;
		}

		if ( !seed_mode && !nospread_mode )
		{
			const auto skeleton = g_shared.lc( ).get_skeleton( *hit_record );
			const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( hit_pawn )->m_pGameSceneNode( );
			const auto hitbox_set = systems::g_hitboxes.query( game_scene_node );

			const systems::hitboxes::entry* best_hb{ nullptr };

			for ( const auto& entry : hitbox_set )
			{
				if ( entry.index == pen.hitbox )
				{
					best_hb = &entry;
					break;
				}
			}

			if ( best_hb && best_hb->bone >= 0 && best_hb->bone < 28 )
			{
				hit_record->apply( );
				const auto hc = g_shared.calculate_hitchance( shoot_position, corrected_angles, *best_hb, skeleton[ best_hb->bone ], ctx.inaccuracy, ctx.spread );
				hit_record->restore( );

				const auto min_hc = static_cast< float >( config.trigger_hitchance.value ) / 100.0f;
				if ( hc < min_hc && !g_shared.is_max_accuracy( ctx.inaccuracy ) )
				{
					this->m_trigger_pending_pawn = 0;
					return;
				}
			}

			const auto delay_ms = static_cast< float >( config.trigger_delay.value );

			if ( this->m_trigger_pending_pawn != hit_pawn )
			{
				this->m_trigger_pending_pawn = hit_pawn;
				this->m_trigger_delay_start = ctx.current_time;
			}

			const auto elapsed_ms = ( ctx.current_time - this->m_trigger_delay_start ) * 1000.0f;
			if ( elapsed_ms < delay_ms )
			{
				return;
			}
		}

		const auto tick_base = reinterpret_cast<CBasePlayerController*>( local.controller )->m_nTickBase( );

		if ( hit_pawn && hit_record && hit_record->bone_count > 0 )
		{
			features::esp::player::g_chams.os( ).push( hit_pawn, hit_record->bones, hit_record->bone_count );
		}
		else if ( hit_pawn )
		{
			features::esp::player::g_chams.os( ).push( hit_pawn );
		}

		const auto record_time = cstypes::tick_fraction::from_value( hit_record->simulation_time / cstypes::tick_interval );
		const auto input_history_size = cmd->csgo_user_cmd.input_history_size( );
		const auto rcs_active = config.rcs.value && aim_punch.length_sqr( ) > 0.0001f;
		const auto punch_x = rcs_active ? aim_punch.x : 0.0f;
		const auto punch_y = rcs_active ? aim_punch.y : 0.0f;

		auto history_angles = math::vector3{ view_angles.x - punch_x, view_angles.y - punch_y, seed_mode ? corrected_angles.z : 0.0f };
		auto nospread_stamp_tick{ 0 };
		auto nospread_stamp_frac{ 0.0f };
		auto nospread_interp_ticks{ 1 };
		auto nospread_eye = shoot_position;

		if ( nospread_mode )
		{
			( void )hitchance::init( );
			auto& shared_ctx = g_shared.ctx( );
			const auto saved_inaccuracy = shared_ctx.inaccuracy;
			auto live_inac = g_shared.get_inaccuracy( true );
			if ( !std::isfinite( live_inac ) || live_inac < 0.0f )
				live_inac = saved_inaccuracy;
			shared_ctx.inaccuracy = live_inac;

			g_shared.sh( ).snapshot( local.pawn, shared_ctx.weapon_services );
			auto eyes = g_shared.sh( ).get_candidates( );
			auto source_eye = shared::shoot_history::eye_candidate{};
			source_eye.position = shoot_position;
			source_eye.is_uninterpolated = true;

			if ( eyes.count > 0 )
			{
				source_eye = eyes.entries[ 0 ];
			}

			nospread_eye = source_eye.position;
			auto aim_angle = math::helpers::calculate_angle( nospread_eye, hit_position );
			auto stamp_tick = tick_base;
			auto stamp_frac{ 0.0f };

			if ( !source_eye.is_uninterpolated )
			{
				auto tick_add = [ ]( int t, float f, int dt, float df )
					{
						f += df;
						auto carry = static_cast< int >( std::floor( f ) );
						f -= static_cast< float >( carry );
						return std::pair{ t + dt + carry, f };
					};

				std::tie( stamp_tick, stamp_frac ) = tick_add( source_eye.player_tick, source_eye.player_frac, source_eye.lerp_ticks_int, source_eye.lerp_ticks_frac );
			}

			if ( stamp_tick <= 0 )
			{
				hitchance::gun_fire_data fire{};
				if ( seed_trigger::safe_fill_gun_fire_data( ctx.weapon, fire ) && fire.ok && fire.tick > 0 )
				{
					stamp_tick = fire.tick;
					stamp_frac = fire.frac;
					if ( hitchance::detail::is_valid_vec( fire.eye ) )
						nospread_eye = fire.eye;
				}
			}

			if ( stamp_tick <= 0 )
				stamp_tick = seed_tick;

			math::vector3 corrected{};
			auto solved = stamp_tick > 0 && g_shared.find_spread_correction( aim_angle, stamp_tick, corrected );
			if ( !solved && stamp_tick > 0 && ctx.weapon )
				solved = seed_nospread::direct_compensate( nospread_eye, aim_angle, stamp_tick, stamp_frac, ctx.weapon, local.pawn, corrected );

			shared_ctx.inaccuracy = saved_inaccuracy;

			if ( !solved )
			{
				this->m_trigger_pending_pawn = 0;
				return;
			}

			aim_angle = corrected;
			history_angles = math::vector3{ aim_angle.x - aim_punch.x, aim_angle.y - aim_punch.y, aim_angle.z };
			nospread_stamp_tick = stamp_tick;
			nospread_stamp_frac = std::clamp( stamp_frac, 0.0f, 0.9999f );
			nospread_interp_ticks = source_eye.is_uninterpolated
				? 1
				: ( std::max )( 1, source_eye.lerp_ticks_int + ( source_eye.lerp_ticks_frac > 0.5f ? 1 : 0 ) );
		}

		g_shared.last_shoot_tick( ) = tick_base;

		auto atk_idx = input_history_size > 0 ? input_history_size - 1 : -1;
		if ( seed_mode || nospread_mode )
		{
			atk_idx = seed_trigger::prefer_tip_hist_index( cmd, cmd->csgo_user_cmd.attack1_start_history_index( ), &view_angles );
			if ( atk_idx < 0 || atk_idx >= input_history_size )
			{
				atk_idx = input_history_size > 0 ? input_history_size - 1 : -1;
			}
		}

		for ( auto i = 0; i < input_history_size; ++i )
		{
			const auto entry = cmd->csgo_user_cmd.mutable_input_history( i );
			if ( !entry )
			{
				continue;
			}

			if ( const auto angles = entry->mutable_view_angles( ) )
			{
				angles->set_x( history_angles.x );
				angles->set_y( history_angles.y );
				if ( seed_mode || nospread_mode )
				{
					angles->set_z( history_angles.z );
				}
			}

			entry->set_render_tick_count( record_time.tick + ( nospread_mode ? nospread_interp_ticks : 1 ) );
			entry->set_render_tick_fraction( 0.0f );

			if ( nospread_mode && nospread_stamp_tick > 0 )
			{
				entry->set_player_tick_count( nospread_stamp_tick );
				entry->set_player_tick_fraction( nospread_stamp_frac );
				if ( i == atk_idx && entry->has_shoot_position( ) )
				{
					if ( auto* sp = entry->mutable_shoot_position( ) )
					{
						sp->set_x( nospread_eye.x );
						sp->set_y( nospread_eye.y );
						sp->set_z( nospread_eye.z );
					}
				}
			}

			if ( seed_mode && i == atk_idx && seed_tick > 0 )
			{
				entry->set_player_tick_count( seed_tick );
				entry->set_player_tick_fraction( std::clamp( seed_frac, 0.0f, 0.9999f ) );
				if ( entry->has_shoot_position( ) )
				{
					if ( auto* sp = entry->mutable_shoot_position( ) )
					{
						sp->set_x( shoot_position.x );
						sp->set_y( shoot_position.y );
						sp->set_z( shoot_position.z );
					}
				}
			}

			if ( entry->has_sv_interp0( ) )
			{
				const auto interp = entry->mutable_sv_interp0( );
				interp->set_src_tick( -1 );
				interp->set_dst_tick( -1 );
				interp->set_frac( 0.0f );
			}

			if ( entry->has_sv_interp1( ) )
			{
				const auto interp = entry->mutable_sv_interp1( );
				interp->set_src_tick( -1 );
				interp->set_dst_tick( -1 );
				interp->set_frac( 0.0f );
			}

			if ( entry->has_cl_interp( ) )
			{
				const auto interp = entry->mutable_cl_interp( );
				interp->set_frac( 0.0f );
			}
		}

		cmd->buttons.value |= cstypes::command_buttons::in_attack;
		cmd->buttons.value_changed |= cstypes::command_buttons::in_attack;
		if ( nospread_mode )
		{
			cmd->buttons.value_scroll |= cstypes::command_buttons::in_attack;
		}

		if ( atk_idx >= 0 )
		{
			cmd->csgo_user_cmd.set_attack1_start_history_index( atk_idx );
		}
		else if ( input_history_size > 0 )
		{
			cmd->csgo_user_cmd.set_attack1_start_history_index( input_history_size - 1 );
		}

		if ( ( seed_mode || nospread_mode ) && ctx.weapon )
		{
			seed_trigger::note_seed_fired( ctx.weapon, local.pawn );
		}

		this->m_trigger_release_time = ctx.current_time + random::hold_duration( );
	}

	void legit::apply_rcs( math::vector3& aim_angle, const math::vector3& aim_punch, int rand_min, int rand_max ) const
	{
		if ( aim_punch.length_sqr( ) < 0.0001f )
		{
			return;
		}

		const auto factor = this->compute_rcs_factor( rand_min, rand_max );

		aim_angle.x -= aim_punch.x * factor;
		aim_angle.y -= aim_punch.y * factor;
		math::helpers::normalize_angles( aim_angle );
	}

	void legit::update_standalone_rcs( const math::vector3& view_angles, const math::vector3& aim_punch, int amount, int rand_min, int rand_max, bool apply, const systems::local::snapshot& local )
	{
		const auto shots_fired = reinterpret_cast<C_CSPlayerPawn*>( local.pawn )->m_iShotsFired();
		if ( shots_fired > 1 )
		{
			const auto factor = this->compute_rcs_factor( rand_min, rand_max );
			const auto scale = static_cast< float >( amount ) / 100.0f;

			const auto punch_scaled = math::vector3
			{
				aim_punch.x * scale * factor,
				aim_punch.y * scale * factor,
				0.0f
			};

			if ( apply )
			{
				auto new_angles = view_angles;
				new_angles.x += this->m_old_punch.x - punch_scaled.x;
				new_angles.y += this->m_old_punch.y - punch_scaled.y;
				math::helpers::normalize_angles( new_angles );

				systems::g_input.set_view_angles( new_angles );
			}

			this->m_old_punch = punch_scaled;
		}
		else
		{
			this->m_old_punch = {};
		}
	}

	float legit::compute_rcs_factor( int rand_min, int rand_max ) const
	{
		const auto seed = static_cast< std::uint32_t >( g_shared.ctx( ).current_time * 1000.0f );
		const auto t = static_cast< float >( seed % 1000 ) / 1000.0f;
		const auto min_scale = static_cast< float >( rand_min ) / 100.0f;
		const auto max_scale = static_cast< float >( rand_max ) / 100.0f;
		return min_scale + ( max_scale - min_scale ) * t;
	}

	void legit::draw_fov( xdraw::draw_list& draw_list, const math::vector3& view_angles, const math::vector3& aim_punch, float fov_degrees, const config::col& color, bool rcs_active ) const
	{
		const auto [screen_w, screen_h] = xdraw::viewport_size( );
		const auto sw = static_cast< float >( screen_w );
		const auto sh = static_cast< float >( screen_h );

		const auto camera_fov_rad = math::helpers::deg_to_rad( systems::g_view.fov( ) );
		const auto aimbot_fov_rad = math::helpers::deg_to_rad( fov_degrees );
		const auto radius = std::tanf( aimbot_fov_rad ) / std::tanf( camera_fov_rad * 0.5f ) * ( sw * 0.5f );

		auto offset_x{ 0.0f };
		auto offset_y{ 0.0f };

		if ( rcs_active )
		{
			const auto punch_magnitude = aim_punch.length_sqr( );
			const auto current_time = g_shared.ctx( ).current_time;

			if ( punch_magnitude > 0.5f )
			{
				this->m_last_significant_punch_time = current_time;
			}

			const auto time_since = current_time - this->m_last_significant_punch_time;
			if ( time_since < 0.3f && punch_magnitude > 0.01f )
			{
				auto corrected = view_angles;
				corrected.x -= aim_punch.x;
				corrected.y -= aim_punch.y;
				math::helpers::normalize_angles( corrected );

				math::vector3 center_dir{}, corrected_dir{};
			math::helpers::angle_vectors_left( view_angles, &center_dir );
				math::helpers::angle_vectors_left( corrected, &corrected_dir );

				const auto render_origin = systems::g_frame_data.origin( );
				const auto cs = systems::g_view.project( render_origin + center_dir * 1000.0f );
				const auto ns = systems::g_view.project( render_origin + corrected_dir * 1000.0f );

				if ( systems::g_view.projection_valid( cs ) && systems::g_view.projection_valid( ns ) )
				{
					offset_x = ( cs.x - ns.x ) * 0.5f;
					offset_y = ( cs.y - ns.y ) * 0.5f;
				}
			}
		}

		const auto cx = sw * 0.5f + offset_x;
		const auto cy = sh * 0.5f + offset_y;
		const auto& c = color.value;

		draw_list.circle( cx, cy, radius, xdraw::color{ c.r, c.g, c.b, c.a }, 1.5f, 64 );
	}

	int legit::hitgroup_to_cfg( int hitgroup )
	{
		static constexpr int table[ ]{ -1, 0, 1, 2, 3, 3, 4, 4 };

	if ( hitgroup < 0 || hitgroup >= static_cast< int >( std::size( table ) ) )
		{
			return -1;
		}

		return table[ hitgroup ];
	}

}


