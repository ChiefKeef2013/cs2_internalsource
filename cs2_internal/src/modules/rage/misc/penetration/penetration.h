#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/common.hpp>

namespace features::combat {

	namespace detail {

		struct bullet_trace_record
		{
			float enter_fraction;
			float exit_fraction;
			float damage_applied;
			int team_at_contact;
			std::uint16_t enter_contact_ix;
			std::uint16_t exit_contact_ix;
			std::uint8_t can_penetrate;
			std::uint8_t pad[ 3 ];
		};

		struct cached_damage_scales
		{
			float ct_head{ 1.0f };
			float t_head{ 1.0f };
			float ct_body{ 1.0f };
			float t_body{ 1.0f };
			std::uint64_t refresh_tick{ 0 };
		};

		inline const shared::penetration::damage_scales& cached_scales( )
		{
			static shared::penetration::damage_scales s{ 1.0f, 1.0f, 1.0f, 1.0f };
			static std::uint64_t next_refresh_ms{ 0 };
			const auto now_ms = static_cast<std::uint64_t>( GetTickCount64( ) );
			if ( now_ms >= next_refresh_ms )
			{
				s.ct_head = CONVAR( "mp_damage_scale_ct_head" )->get<float>( );
				s.t_head = CONVAR( "mp_damage_scale_t_head" )->get<float>( );
				s.ct_body = CONVAR( "mp_damage_scale_ct_body" )->get<float>( );
				s.t_body = CONVAR( "mp_damage_scale_t_body" )->get<float>( );
				next_refresh_ms = now_ms + 1000;
			}
			return s;
		}

	}

	void shared::penetration::prepare( std::uintptr_t weapon_vdata, std::uintptr_t weapon )
	{
		if ( !weapon_vdata || !weapon )
		{
			return;
		}

		const auto vdata = reinterpret_cast<CCSWeaponBaseVData*>(weapon_vdata);
		this->m_weapon_data = weapon_data
		{
			.damage = static_cast< float >( vdata->m_nDamage() ),
			.penetration = vdata->m_flPenetration(),
			.range_modifier = vdata->m_flRangeModifier(),
			.range = vdata->m_flRange(),
			.armor_ratio = vdata->m_flArmorRatio(),
			.headshot_multiplier = vdata->m_flHeadshotMultiplier()
		};
	}

	shared::penetration::run_context shared::penetration::prepare_target( std::uintptr_t target_pawn, const lagcomp::record* record ) const
	{
		run_context ctx{};
		ctx.target_pawn = target_pawn;
		ctx.record = record;
		if ( record && record->game_scene_node )
		{
			ctx.hitboxes = systems::g_hitboxes.query( record->game_scene_node );
		}

		ctx.target_armor = reinterpret_cast<C_CSPlayerPawn*>(target_pawn)->m_ArmorValue();
		ctx.target_team = reinterpret_cast<C_BaseEntity*>(target_pawn)->m_iTeamNum();

		if ( ctx.target_armor > 0 )
		{
			const auto services = reinterpret_cast<C_BasePlayerPawn*>(target_pawn)->m_pItemServices();
			if ( services )
			{
				ctx.has_helmet = reinterpret_cast<CCSPlayer_ItemServices*>(services)->m_bHasHelmet();
			}
		}

		ctx.scales = detail::cached_scales( );

		ctx.armor_ratio = this->m_weapon_data.armor_ratio;
		ctx.headshot_multiplier = this->m_weapon_data.headshot_multiplier;

		return ctx;
	}

	bool shared::penetration::run( const math::vector3& start, const math::vector3& end, const run_context& ctx, std::uintptr_t local_pawn, int local_team, result& out ) const
	{
		if ( this->m_weapon_data.damage <= 0.0f )
		{
			return false;
		}

		const auto direction = ( end - start ).normalized( );
		const auto trace_delta = direction * this->m_weapon_data.range;

		auto filter = systems::g_tracing.make_filter( local_pawn, 0x1c300b, 3, 15 );
		thread_local systems::tracing::trace_data trace_storage{};
		trace_storage = {};
		auto* trace = &trace_storage;
		trace->array_pointer = &trace->elements;
		trace->hit_array_pointer = &trace->hit_elements;

		g_shared.m_current_autowall_record = ctx.record;
		g_shared.m_autowalling = true;

		systems::g_tracing.setup_trace( trace, start, trace_delta, filter, 4, true );

		g_shared.m_autowalling = false;
		g_shared.m_current_autowall_record = nullptr;

		const auto num_hits = trace->num_hits;
		const auto hit_array = reinterpret_cast< std::uintptr_t >( trace->hit_array_pointer );

		if ( num_hits <= 0 )
		{
			out = {};
			return false;
		}

		const auto surface_array = reinterpret_cast< std::uintptr_t >( trace->array_pointer );

		memory::call<void> (PATTERN(PATTERN_TRACE_BULLET), trace, this->m_weapon_data.damage, this->m_weapon_data.penetration, this->m_weapon_data.range_modifier, 4, local_team, static_cast<std::uintptr_t>(0));

		auto actual_hitbox{ -1 };
		auto closest_hitbox_fraction{ 1.0f };
		if ( ctx.record )
		{
			for ( const auto& hitbox : ctx.hitboxes )
			{
				if ( hitbox.bone < 0 || hitbox.bone >= ctx.record->bone_count )
				{
					continue;
				}

				const auto& bone = ctx.record->bones[ hitbox.bone ];
				auto fraction{ 1.0f };
				auto intersects{ false };

				if ( hitbox.radius > 0.001f )
				{
					const auto capsule_start = bone.rotation.rotate_vector( hitbox.mins ) + bone.position;
					const auto capsule_end = bone.rotation.rotate_vector( hitbox.maxs ) + bone.position;
					intersects = g_shared.ray_vs_capsule( start, trace_delta, capsule_start, capsule_end, hitbox.radius, fraction );
				}
				else
				{
					auto inverse = bone.rotation;
					inverse.x = -inverse.x;
					inverse.y = -inverse.y;
					inverse.z = -inverse.z;

					const auto local_origin = inverse.rotate_vector( start - bone.position );
					const auto local_delta = inverse.rotate_vector( trace_delta );
					auto entry{ 0.0f };
					auto exit{ 1.0f };

					const auto intersect_axis = [ & ]( float origin, float delta, float minimum, float maximum )
						{
						if ( std::fabsf( delta ) < 1.0e-8f )
							{
								return origin >= minimum && origin <= maximum;
							}

							auto first = ( minimum - origin ) / delta;
							auto second = ( maximum - origin ) / delta;
							if ( first > second ) std::swap( first, second );
							entry = std::max( entry, first );
							exit = std::min( exit, second );
							return entry <= exit;
						};

					intersects = intersect_axis( local_origin.x, local_delta.x, hitbox.mins.x, hitbox.maxs.x ) &&
						intersect_axis( local_origin.y, local_delta.y, hitbox.mins.y, hitbox.maxs.y ) &&
						intersect_axis( local_origin.z, local_delta.z, hitbox.mins.z, hitbox.maxs.z );
					fraction = entry;
				}

				if ( intersects && fraction < closest_hitbox_fraction )
				{
					closest_hitbox_fraction = fraction;
					actual_hitbox = hitbox.index;
				}
			}
		}

		auto penetrated{ false };

		for ( auto i = 0; i < num_hits; ++i )
		{
			auto hit = reinterpret_cast< detail::bullet_trace_record* >( hit_array + i * sizeof( detail::bullet_trace_record ) );
			const auto damage = *reinterpret_cast< float* >( reinterpret_cast< std::uintptr_t >( hit ) + 8 );

			if ( damage <= 0.0f )
			{
				break;
			}

			if ( ( hit->can_penetrate & 1 ) != 0 )
			{
				penetrated = true;

				if ( *reinterpret_cast< float* >( reinterpret_cast< std::uintptr_t >( hit ) + 4 ) == 1.0f )
				{
					break;
				}

				continue;
			}

			const auto trace_holder = surface_array + sizeof( systems::tracing::trace_array_element ) * ( hit->enter_contact_ix & 0x7fff );
			const auto hit_handle = memory::read<std::uint32_t>( trace_holder + 0x2c );
			const auto hit_entity = systems::g_entities.lookup( hit_handle );

			if ( !hit_entity || hit_entity != ctx.target_pawn )
			{
				continue;
			}

			if ( actual_hitbox < 0 )
			{
				continue;
			}

			out.hitbox = actual_hitbox;
			out.hitgroup = systems::g_hitboxes.hitgroup_from_hitbox( actual_hitbox );
			out.penetrated = penetrated;
			out.damage = damage;

			this->scale_damage( out.hitgroup, ctx.target_armor, ctx.has_helmet, ctx.target_team, ctx.armor_ratio, ctx.headshot_multiplier, ctx.scales, out.damage );

			return true;
		}

		out = {};
		return false;
	}

	bool shared::penetration::can( const math::vector3& start, const math::vector3& direction, float& out_damage, const systems::local::snapshot& local ) const
	{
		out_damage = 0.0f;

		if ( this->m_weapon_data.damage <= 0.0f || this->m_weapon_data.penetration <= 0.0f )
		{
			return false;
		}

		const auto local_team = reinterpret_cast<C_BaseEntity*>(local.pawn)->m_iTeamNum();
		const auto trace_delta = direction * this->m_weapon_data.range;

		auto filter = systems::g_tracing.make_filter( local.pawn, k_pen_crosshair_brush_mask, 3, 15 );
		thread_local systems::tracing::trace_data trace_storage{};
		trace_storage = {};
		auto* trace = &trace_storage;
		trace->array_pointer = &trace->elements;
		trace->hit_array_pointer = &trace->hit_elements;

		systems::g_tracing.setup_trace( trace, start, trace_delta, filter, 4, true );

		const auto num_hits = trace->num_hits;

		if ( num_hits <= 0 )
		{
			return false;
		}

		const auto hit_array = reinterpret_cast< std::uintptr_t >( trace->hit_array_pointer );

		memory::call<void> (PATTERN(PATTERN_TRACE_BULLET), trace, this->m_weapon_data.damage, this->m_weapon_data.penetration, this->m_weapon_data.range_modifier, 4, local_team, static_cast<std::uintptr_t>(0));

		for ( auto i = 0; i < num_hits; ++i )
		{
			auto hit = reinterpret_cast< detail::bullet_trace_record* >( hit_array + i * sizeof( detail::bullet_trace_record ) );
			const auto damage = hit->damage_applied;

			if ( damage <= 0.0f )
			{
				break;
			}

			if ( ( hit->can_penetrate & 1 ) != 0 )
			{
				if ( hit->exit_fraction == 1.0f )
				{
					break;
				}

				out_damage = damage;
				return true;
			}
		}

		return false;
	}

	float shared::penetration::get_max_damage( int hitgroup, int target_armor, bool has_helmet, int target_team ) const
	{
		if ( this->m_weapon_data.damage <= 0.0f )
		{
			return 0.0f;
		}

		const auto& scales = detail::cached_scales( );

		auto damage = this->m_weapon_data.damage;
		this->scale_damage( hitgroup, target_armor, has_helmet, target_team, this->m_weapon_data.armor_ratio, this->m_weapon_data.headshot_multiplier, scales, damage );
		return damage;
	}

	void shared::penetration::scale_damage( int hitgroup, int armor, bool has_helmet, int team, float armor_ratio, float headshot_multiplier, const damage_scales& scales, float& damage ) const
	{
		const auto is_ct = ( team == 3 );
		const auto head_scale = is_ct ? scales.ct_head : scales.t_head;
		const auto body_scale = is_ct ? scales.ct_body : scales.t_body;

		switch ( hitgroup )
		{
		case 1:
			damage *= headshot_multiplier * head_scale;
			break;
		case 2:
		case 4:
		case 5:
		case 8:
			damage *= body_scale;
			break;
		case 3:
			damage *= 1.25f * body_scale;
			break;
		case 6:
		case 7:
			damage *= 0.75f * body_scale;
			break;
		default:
			break;
		}

		const auto is_head = ( hitgroup == 1 );
		const auto is_armored = ( hitgroup >= 1 && hitgroup <= 5 ) || ( hitgroup == 8 );

		if ( armor <= 0 || !is_armored || ( is_head && !has_helmet ) )
		{
			damage = std::floor( damage );
			return;
		}

		constexpr auto armor_bonus{ 0.5f };
		const auto armor_ratio_scaled = armor_ratio * 0.5f;

		auto damage_to_health = damage * armor_ratio_scaled;
		auto damage_to_armor = ( damage - damage_to_health ) * armor_bonus;

		if ( damage_to_armor > static_cast< float >( armor ) )
		{
			damage_to_health = damage - ( static_cast< float >( armor ) / armor_bonus );
		}

		damage = std::floor( damage_to_health );
	}

}



