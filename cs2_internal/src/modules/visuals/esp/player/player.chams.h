#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <modules/visuals/esp/primitive_buffer.hpp>

namespace features::esp::player {

	namespace {

		struct bone_matrix {
			float row0[ 4 ]{};
			float row1[ 4 ]{};
			float row2[ 4 ]{};
		};

		[[nodiscard]] bool scene_object_alive( std::uintptr_t scene_object )
		{
			return detail::scene_object_live( scene_object );
		}

		[[nodiscard]] bool ghost_ready( std::uintptr_t scene_object )
		{
			if ( !scene_object_alive( scene_object ) )
			{
				return false;
			}

			const auto bone_count = memory::safe_read<int>( scene_object + 0xd0 );
			const auto render_bones = memory::safe_read<std::uintptr_t>( scene_object + 0xd8 );
			return bone_count && *bone_count > 0 && render_bones && *render_bones;
		}

		void destroy_scene_object( std::uintptr_t scene_object, const char* )
		{
			if ( !scene_object )
			{
				return;
			}

			if ( detail::scene_object_freed( scene_object ) )
			{
				return;
			}

			if ( !addresses::globals::scene_system )
			{
				return;
			}

			memory::call_vfunc<void>( addresses::globals::scene_system, 16, scene_object );
		}

	}

	bool chams::on_generate_primitives( std::uintptr_t owner_entity, std::uint32_t owner_hash, std::uintptr_t scene_object, std::uintptr_t primitive_buffer, void( __fastcall* original_fn )( std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t ), std::uintptr_t a1, std::uintptr_t scene_view )
	{
		if ( !memory::is_game_ptr( owner_entity ) || !scene_object_alive( scene_object ) )
		{
			return false;
		}

		const auto is_player = owner_hash == "C_CSPlayerPawn"_hash || features::visuals::model_preview::is_preview_player( owner_hash );
		const auto is_arms = owner_hash == "C_CS2HudModelArms"_hash;
		const auto is_weapon = owner_hash == "C_CS2HudModelWeapon"_hash;

		auto life_state = std::uint8_t{ 0 };
		if ( is_player && !features::visuals::model_preview::is_preview_player( owner_hash ) )
		{
			const auto life = memory::safe_read<std::uint8_t>(
				owner_entity + SCHEMA_OFFSET( "C_BaseEntity", "m_lifeState"_hash ) );
			if ( life )
			{
				life_state = *life;
			}
		}

		const auto dying = life_state == 1;

		const auto is_local_attachment = [ & ]( std::uintptr_t view_pawn ) -> bool
			{
				if ( !view_pawn || !owner_entity )
				{
					return false;
				}

				if ( owner_entity == view_pawn )
				{
					return true;
				}

				auto node = reinterpret_cast<C_BaseEntity*>( owner_entity )->m_pGameSceneNode( );
				for ( int depth = 0; depth < 8 && memory::is_game_ptr( node ); ++depth )
				{
					const auto owner = reinterpret_cast<CGameSceneNode*>( node )->m_pOwner( );
					if ( owner == view_pawn )
					{
						return true;
					}

					node = reinterpret_cast<CGameSceneNode*>( node )->m_pParent( );
				}

				return false;
			};

		const auto apply_config = [ & ]( const settings::esp::chams_config& cfg, std::uintptr_t target_scene_obj, bool force_original = false, bool skip_occluded = false )
			{
				if ( !skip_occluded && cfg.occluded_material.value != settings::esp::cham_ids::count )
				{
					this->apply_layer( primitive_buffer, original_fn, a1, target_scene_obj, scene_view, cfg.occluded_color, cfg.occluded_material, true );
				}

				if ( cfg.visible_material.value != settings::esp::cham_ids::count )
				{
					this->apply_layer( primitive_buffer, original_fn, a1, target_scene_obj, scene_view, cfg.visible_color, cfg.visible_material, false );
				}

				if ( cfg.overlay.enabled.value )
				{
					this->apply_overlay( primitive_buffer, original_fn, a1, target_scene_obj, scene_view, cfg.overlay.color, cfg.overlay.material );
				}
			};

		if ( !is_player && !is_arms && !is_weapon )
		{
			if ( !settings::g_esp.m_viewmodel.weapon.enabled.value )
			{
				return false;
			}

			const auto vdata = memory::read<std::uintptr_t>(
				owner_entity + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );
			if ( !vdata )
			{
				return false;
			}

			const auto preview_pawn = features::visuals::model_preview::preview_player( );
			if ( preview_pawn && is_local_attachment( preview_pawn ) )
			{
				apply_config( settings::g_esp.m_viewmodel.weapon, scene_object );
				return true;
			}

			if ( !settings::g_misc.m_camera.thirdperson.value || !is_local_attachment( systems::g_local.get( ).view_pawn( ) ) )
			{
				return false;
			}

			apply_config( settings::g_esp.m_viewmodel.weapon, scene_object );
			return true;
		}

		if ( is_arms || is_weapon )
		{
			if ( is_arms )
			{
				const auto& arms = settings::g_esp.m_viewmodel.arms;
				const auto& gloves = settings::g_esp.m_viewmodel.gloves;

				if ( !arms.enabled.value && !gloves.enabled.value )
				{
					return false;
				}

				if ( arms.enabled.value )
				{
					apply_config( arms, scene_object );
				}

				if ( gloves.enabled.value )
				{
					apply_config( gloves, scene_object );
				}

				return true;
			}

			const auto& cfg = settings::g_esp.m_viewmodel.weapon;
			if ( !cfg.enabled.value )
			{
				return false;
			}

			apply_config( cfg, scene_object );
			return true;
		}

		const auto local = systems::g_local.get( );
		const auto& chams_cfg = settings::g_esp.m_player.m_chams;

	if ( features::visuals::model_preview::is_preview_player( owner_hash ) )
		{
			const settings::esp::chams_config* preview_target = &chams_cfg.enemy;
		switch ( features::visuals::model_preview::preview_chams_group( ) )
			{
			case 1: preview_target = &chams_cfg.team; break;
			case 2: preview_target = &chams_cfg.local; break;
			default: preview_target = &chams_cfg.enemy; break;
			}

			if ( !preview_target->enabled.value )
				return false;

			{
				const auto flags = memory::safe_read<std::uint8_t>( scene_object + 0x78 );
				if ( flags )
				{
					(void) memory::safe_write<std::uint8_t>(
						scene_object + 0x78,
						static_cast< std::uint8_t >( *flags & ~( 1u << 3 ) ) );
				}
			}

			apply_config( *preview_target, scene_object );
			return true;
		}

		const auto team = reinterpret_cast<C_BaseEntity*>( owner_entity )->m_iTeamNum( );
		const auto health = reinterpret_cast<C_BaseEntity*>( owner_entity )->m_iHealth( );

		const auto is_other_team = local.is_this_other_team( team );
		const auto is_local = owner_entity == local.view_pawn( );
		const auto is_dead = health <= 0;

		if ( is_player && is_other_team && !is_dead && chams_cfg.backtrack.enabled.value )
		{
			const auto bt_scene_object = this->m_backtrack.get_scene_object( owner_entity );
			const auto bt_generation = this->m_backtrack.get_generation( owner_entity );
			if ( bt_scene_object &&
				this->m_backtrack.matches( owner_entity, bt_scene_object, bt_generation ) &&
				ghost_ready( bt_scene_object ) )
			{
				const auto before = detail::read_primitive_buffer( primitive_buffer );
				const auto prev_count = before ? before->count( ) : -1;

				apply_config( chams_cfg.backtrack, bt_scene_object, true );

				const auto after = detail::read_primitive_buffer( primitive_buffer );
				const auto new_count = after ? after->count( ) : -1;
				if ( after && prev_count >= 0 && new_count > prev_count )
				{
					for ( auto i = prev_count; i < new_count; ++i )
					{
						detail::mark_primitive_last( after->at( i ) );
					}
				}
			}
		}

		if ( is_player && is_other_team && chams_cfg.onshot.enabled.value )
		{
			const auto os_obj = this->m_onshot.get_scene_object( owner_entity );
			if ( os_obj && os_obj != scene_object && this->m_onshot.has_active( owner_entity ) )
			{
				this->apply_onshot( owner_entity, os_obj, primitive_buffer, original_fn, a1, scene_view );
			}
		}

		if ( dying )
		{
			return true;
		}

		const settings::esp::chams_config* target{ nullptr };

		if ( is_dead )
		{
			if ( is_local )
			{
				target = &chams_cfg.local_ragdoll;
			}
			else if ( is_other_team )
			{
				target = &chams_cfg.enemy_ragdoll;
			}
			else
			{
				target = &chams_cfg.team_ragdoll;
			}
		}
		else
		{
			if ( is_local )
			{
				target = &chams_cfg.local;
			}
			else if ( is_other_team )
			{
				target = &chams_cfg.enemy;
			}
			else
			{
				target = &chams_cfg.team;
			}
		}

		if ( !target || !target->enabled.value )
		{
			return false;
		}


		{
			const auto flags = memory::safe_read<std::uint8_t>( scene_object + 0x78 );
			if ( flags ) {
				(void) memory::safe_write<std::uint8_t>(
					scene_object + 0x78,
					static_cast<std::uint8_t>( *flags & ~( 1u << 3 ) ) );
			}
		}

		if ( is_local )
		{
			if ( misc::g_local_alpha.is_alpha_changed( ) )
			{
				this->apply_clone( primitive_buffer, original_fn, a1, scene_object, scene_view, systems::materials::clone_type::translucent );

				if ( target->overlay.enabled.value )
				{
					this->apply_overlay( primitive_buffer, original_fn, a1, scene_object, scene_view, target->overlay.color, target->overlay.material );
				}
			}
			else
			{
				apply_config( *target, scene_object );
			}

			return true;
		}

		apply_config( *target, scene_object, false, false );
		return true;
	}

	void chams::on_sort_primitives( std::uintptr_t entries, std::uint32_t count )
	{
		if ( !count || !entries || count > ( 1u << 20 ) )
		{
			return;
		}

		const auto overlay_mat_count = this->m_overlay_material_count.load( std::memory_order_acquire );
		if ( overlay_mat_count <= 0 )
		{
			return;
		}

		const auto total = static_cast< int >( count );
		if ( total <= 1 )
		{
			return;
		}

		std::vector<detail::mesh_primitive> sorted;
		sorted.reserve( total );

		for ( auto i = 0; i < total; ++i )
		{
			const auto primitive = memory::safe_read<detail::mesh_primitive>(
				entries + static_cast<std::size_t>( i ) * detail::primitive_size );
			if ( !primitive ) {
				return;
			}

			sorted.push_back( *primitive );
		}

		const auto overlay_begin = std::stable_partition(
			sorted.begin(), sorted.end(), [ this ]( const auto& primitive ) {
				return !this->is_overlay_material( primitive.material );
			} );
		const auto overlay_count = static_cast<int>(
		std::distance( overlay_begin, sorted.end() ) );

		if ( overlay_count <= 0 || overlay_count >= total )
		{
			return;
		}

		for ( auto i = 0; i < total; ++i )
		{
			if ( !memory::safe_write<detail::mesh_primitive>(
					entries + static_cast<std::size_t>( i ) * detail::primitive_size,
					sorted[ i ] ) ) {
				return;
			}
		}
	}

	void chams::backtrack::flush_deferred( )
	{
		if ( this->m_destroy_delay > 0 )
		{
			--this->m_destroy_delay;
			return;
		}

		std::vector<std::uintptr_t> dying;
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			dying = this->m_deferred_destroy;
		}

		for ( const auto scene_object : dying )
		{
			destroy_scene_object( scene_object, "chams.bt" );
		}

		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			for ( const auto scene_object : dying )
			{
				for ( auto it = this->m_deferred_destroy.begin( ); it != this->m_deferred_destroy.end( ); ++it )
				{
					if ( *it == scene_object )
					{
						this->m_deferred_destroy.erase( it );
						break;
					}
				}
			}
		}
	}

	void chams::backtrack::defer_destroy( object& obj )
	{
		if ( obj.scene_object )
		{
			this->m_deferred_destroy.push_back( obj.scene_object );
		}

		obj.scene_object = 0;
		obj.active = false;
		obj.generation = 0;
		obj.applied_tick = -1;
	}

	void chams::backtrack::update( )
	{
		const auto& cfg = settings::g_esp.m_player.m_chams;
		const auto local = systems::g_local.get( );

		if ( !local.is_alive || !cfg.backtrack.enabled.value )
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			for ( auto& [pawn, obj] : this->m_objects )
			{
				this->defer_destroy( obj );
			}
			this->m_objects.clear( );
			this->m_destroy_delay = 3;
			return;
		}

		this->flush_deferred( );

		const auto players = systems::g_entities.get_by_type( systems::entities::type::player );

		std::unordered_set<std::uintptr_t> valid_pawns;
		std::unordered_set<std::uintptr_t> keep;
		std::vector<std::uintptr_t> need_create;
		std::vector<std::pair<std::uintptr_t, combat::shared::lagcomp::record>> bone_updates;

		{
			std::lock_guard<std::mutex> lock( this->m_mutex );

			for ( const auto& p : players )
			{
				if ( !p.ptr || p.ptr == local.controller )
				{
					continue;
				}

				const auto pawn = systems::g_entities.player_pawn( p.ptr );
				if ( pawn && pawn != local.pawn )
				{
					valid_pawns.insert( pawn );
				}
			}

			for ( auto it = this->m_objects.begin( ); it != this->m_objects.end( ); )
			{
				if ( !valid_pawns.contains( it->first ) )
				{
					this->defer_destroy( it->second );
					it = this->m_objects.erase( it );
				}
				else
				{
					++it;
				}
			}

			for ( const auto& p : players )
			{
				if ( !p.ptr || p.ptr == local.controller )
				{
					continue;
				}

				if ( !reinterpret_cast< CCSPlayerController* >( p.ptr )->m_bPawnIsAlive( ) )
				{
					continue;
				}

				const auto pawn = systems::g_entities.player_pawn( p.ptr );
				if ( !pawn || pawn == local.pawn )
				{
					continue;
				}

				const auto team = reinterpret_cast< C_BaseEntity* >( pawn )->m_iTeamNum( );
				if ( !local.is_this_other_team( team ) )
				{
					continue;
				}

				const auto health = reinterpret_cast< C_BaseEntity* >( pawn )->m_iHealth( );
				if ( health <= 0 )
				{
					auto it = this->m_objects.find( pawn );
					if ( it != this->m_objects.end( ) )
					{
						this->defer_destroy( it->second );
						this->m_objects.erase( it );
					}
					continue;
				}

				const auto oldest = combat::g_shared.lc( ).get_oldest_was_valid( pawn );
				if ( !oldest || oldest->bone_count <= 0 )
				{
					auto it = this->m_objects.find( pawn );
					if ( it != this->m_objects.end( ) )
					{
						this->defer_destroy( it->second );
						this->m_objects.erase( it );
					}
					continue;
				}

				const auto game_scene_node = reinterpret_cast< C_BaseEntity* >( pawn )->m_pGameSceneNode( );
				if ( game_scene_node )
				{
					if ( oldest->origin.distance( reinterpret_cast< CGameSceneNode* >( game_scene_node )->m_vecAbsOrigin( ) ) < 0.25f )
					{
						auto it = this->m_objects.find( pawn );
						if ( it != this->m_objects.end( ) )
						{
							this->defer_destroy( it->second );
							this->m_objects.erase( it );
						}
						continue;
					}
				}

				keep.insert( pawn );
				auto& obj = this->m_objects[ pawn ];
				if ( !obj.scene_object )
				{
					need_create.push_back( pawn );
				}
				else if ( !scene_object_alive( obj.scene_object ) )
				{
					this->defer_destroy( obj );
					need_create.push_back( pawn );
				}
				else if ( obj.applied_tick != oldest->tick || !obj.active )
				{
					bone_updates.emplace_back( pawn, *oldest );
				}
				else
				{
					obj.active = true;
				}
			}

			for ( auto it = this->m_objects.begin( ); it != this->m_objects.end( ); )
			{
				if ( !keep.contains( it->first ) )
				{
					this->defer_destroy( it->second );
					it = this->m_objects.erase( it );
				}
				else
				{
					++it;
				}
			}
		}

		for ( const auto pawn : need_create )
		{
			object created{};
			created.create( pawn );
			if ( !created.scene_object )
			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				this->m_objects.erase( pawn );
				continue;
			}

			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto& obj = this->m_objects[ pawn ];
				if ( obj.scene_object && obj.scene_object != created.scene_object )
				{
					this->defer_destroy( obj );
				}
				obj.scene_object = created.scene_object;
				obj.pawn = pawn;
				obj.active = false;
				obj.generation = this->m_generation_clock++;
				obj.applied_tick = -1;
				if ( this->m_generation_clock == 0 )
				{
					this->m_generation_clock = 1;
				}
			}

			const auto oldest = combat::g_shared.lc( ).get_oldest_was_valid( pawn );
			if ( oldest && oldest->bone_count > 0 )
			{
				object tmp{};
				tmp.scene_object = created.scene_object;
				if ( tmp.setup_bones( oldest->bones, oldest->bone_count ) )
				{
					std::lock_guard<std::mutex> lock( this->m_mutex );
					auto it = this->m_objects.find( pawn );
					if ( it != this->m_objects.end( ) && it->second.scene_object == created.scene_object )
					{
						it->second.applied_tick = oldest->tick;
						it->second.active = true;
					}
				}
			}
		}

		for ( const auto& [pawn, oldest] : bone_updates )
		{
			std::uintptr_t scene_object{};
			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto it = this->m_objects.find( pawn );
				if ( it == this->m_objects.end( ) || !it->second.scene_object )
				{
					continue;
				}
				scene_object = it->second.scene_object;
			}

			if ( !scene_object || !scene_object_alive( scene_object ) )
			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto it = this->m_objects.find( pawn );
				if ( it != this->m_objects.end( ) )
				{
					this->defer_destroy( it->second );
					this->m_objects.erase( it );
				}
				continue;
			}

			object tmp{};
			tmp.scene_object = scene_object;
			if ( tmp.setup_bones( oldest.bones, oldest.bone_count ) )
			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto it = this->m_objects.find( pawn );
					if ( it != this->m_objects.end( ) && it->second.scene_object == scene_object )
					{
						it->second.applied_tick = oldest.tick;
						it->second.active = true;
					}
			}
		}
	}

	void chams::backtrack::shutdown( bool destroy_objects )
	{
		std::vector<std::uintptr_t> dying;
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			if ( destroy_objects )
			{
				for ( auto& [pawn, obj] : this->m_objects )
				{
					if ( obj.scene_object )
					{
						dying.push_back( obj.scene_object );
					}
				}
				dying.insert( dying.end( ), this->m_deferred_destroy.begin( ), this->m_deferred_destroy.end( ) );
			}
			this->m_objects.clear( );
			this->m_deferred_destroy.clear( );
		}

		if ( !destroy_objects )
			return;

		for ( const auto scene_object : dying )
		{
			destroy_scene_object( scene_object, "chams.bt" );
		}
	}

	bool chams::backtrack::has_active( std::uintptr_t pawn ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );
		auto it = this->m_objects.find( pawn );
		return it != this->m_objects.end( ) && it->second.scene_object && it->second.active;
	}

	std::uintptr_t chams::backtrack::get_scene_object( std::uintptr_t pawn ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );
		auto it = this->m_objects.find( pawn );
		if ( it != this->m_objects.end( ) && it->second.active )
		{
			return it->second.scene_object;
		}
		return 0;
	}

	std::uint32_t chams::backtrack::get_generation( std::uintptr_t pawn ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );
		auto it = this->m_objects.find( pawn );
		if ( it != this->m_objects.end( ) && it->second.active )
		{
			return it->second.generation;
		}
		return 0;
	}

	bool chams::backtrack::matches( std::uintptr_t pawn, std::uintptr_t scene_object, std::uint32_t generation ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );
		auto it = this->m_objects.find( pawn );
		return it != this->m_objects.end( )
			&& it->second.active
			&& it->second.scene_object == scene_object
			&& it->second.generation == generation
			&& generation != 0;
	}

	bool chams::backtrack::object::usable( ) const
	{
		return ghost_ready( this->scene_object );
	}

	void chams::backtrack::object::create( std::uintptr_t target_pawn )
	{
		this->pawn = target_pawn;
		this->scene_object = 0;
		this->active = false;
		this->generation = 0;
		this->applied_tick = -1;

		if ( !memory::is_game_ptr( target_pawn ) || !addresses::globals::mesh_system )
		{
			return;
		}

		const auto node_off = SCHEMA_OFFSET( "C_BaseEntity", "m_pGameSceneNode"_hash );
		if ( !node_off )
		{
			return;
		}

		const auto node = memory::safe_read<std::uintptr_t>( target_pawn + node_off );
		if ( !node || !memory::is_game_ptr( *node ) )
		{
			return;
		}

		const auto game_scene_node = *node;

		auto temp{ 0 };
		int world_group_id = 0;
		bool world_group_resolved = false;

		const auto wg_id_pattern = PATTERN( PATTERN_GET_WORLD_GROUP_ID );
		if ( wg_id_pattern )
		{
			const auto wg_ptr = memory::call<int*>( wg_id_pattern, game_scene_node, &temp );
			if ( wg_ptr )
			{
				world_group_id = *wg_ptr;
				world_group_resolved = true;
			}
		}

		if ( !world_group_resolved )
		{
			const auto identity = memory::safe_read<std::uintptr_t>( target_pawn + 0x10 );
			if ( identity && memory::is_game_ptr( *identity ) )
			{
				if ( const auto wg_id = memory::safe_read<std::uint32_t>( *identity + 0x38 ); wg_id )
				{
					world_group_id = static_cast< int >( *wg_id );
					world_group_resolved = true;
				}
			}
		}

		if ( !world_group_resolved )
		{
			return;
		}

		const auto render_game_system = memory::read<std::uintptr_t>( addresses::globals::render_game_system_storage );
		if ( !memory::is_game_ptr( render_game_system ) )
		{
			return;
		}

		auto world_group_handle = 0ull;
		const auto wg_handle_pattern = PATTERN( PATTERN_GET_WORLD_GROUP_HANDLE );
		if ( wg_handle_pattern )
		{
			world_group_handle = memory::call<std::uintptr_t>( wg_handle_pattern, render_game_system, world_group_id );
		}

		if ( !world_group_handle && wg_handle_pattern )
		{
			const auto identity = memory::safe_read<std::uintptr_t>( target_pawn + 0x10 );
			if ( identity && memory::is_game_ptr( *identity ) )
			{
				if ( const auto wg_id = memory::safe_read<std::uint32_t>( *identity + 0x38 ); wg_id )
				{
					world_group_id = static_cast< int >( *wg_id );
					world_group_handle = memory::call<std::uintptr_t>( wg_handle_pattern, render_game_system, world_group_id );
				}
			}
		}

		if ( !world_group_handle )
		{
			return;
		}

		const auto flags = ( world_group_id != 0 ) ? 0x2000000000ll : 0x2000000008ll;
		const auto model_state_off = SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash );
		const auto model_handle_off = SCHEMA_OFFSET( "CModelState", "m_hModel"_hash );
		const auto node_to_world_off = SCHEMA_OFFSET( "CGameSceneNode", "m_nodeToWorld"_hash );
		if ( !model_state_off || !model_handle_off || !node_to_world_off )
		{
			return;
		}

		const auto model_handle = memory::safe_read<std::uintptr_t>( game_scene_node + model_state_off + model_handle_off );
		if ( !model_handle || !memory::is_game_ptr( *model_handle ) )
		{
			return;
		}

		const auto node_to_world = game_scene_node + node_to_world_off;
		alignas( 16 ) std::uint8_t copy_bytes[ 32 ]{};
		const auto row0 = memory::safe_read<__m128>( node_to_world );
		const auto row1 = memory::safe_read<__m128>( node_to_world + 16 );
		if ( !row0 || !row1 )
		{
			return;
		}
	std::memcpy( copy_bytes, &*row0, 16 );
		std::memcpy( copy_bytes + 16, &*row1, 16 );

		this->scene_object = memory::call_vfunc<std::uintptr_t>( addresses::globals::mesh_system, 20, *model_handle, copy_bytes, "AnimatableSceneObjectDesc", flags, 0x4100000001ll, world_group_handle );
		if ( !this->scene_object )
		{
			return;
		}

		( void )memory::safe_write<std::uint32_t>( this->scene_object + 0xc0, detail::k_managed_scene_owner );

		const auto model_data = memory::safe_read<std::uintptr_t>( *model_handle );
		if ( model_data && *model_data )
		{
			const auto f16 = memory::safe_read<std::uint32_t>( *model_data + 16 );
			const auto f20 = memory::safe_read<std::uint32_t>( *model_data + 20 );
			const auto lod_opt = memory::safe_read<std::uint8_t>( this->scene_object + 0x9a );
			if ( f16 && f20 && lod_opt )
			{
				const auto has_force_lod = ( *f16 & 0x400 ) != 0 || ( *f20 & 0x400 ) != 0;
				const auto lod = has_force_lod ? static_cast< std::uint8_t >( *lod_opt | 0x10 ) : static_cast< std::uint8_t >( *lod_opt & 0xef );
				( void )memory::safe_write( this->scene_object + 0x9a, lod );
			}
		}

		this->active = false;
	}

	bool chams::backtrack::object::setup_bones( const systems::bones::data* bones, int count ) const
	{
		if ( !this->scene_object || !bones || count <= 0 )
		{
			return false;
		}

		if ( !scene_object_alive( this->scene_object ) )
		{
			return false;
		}

		const auto obj_bone_count = memory::safe_read<int>( this->scene_object + 0xd0 );
		const auto render_bones = memory::safe_read<std::uintptr_t>( this->scene_object + 0xd8 );
		if ( !obj_bone_count || *obj_bone_count <= 0 || !render_bones || !*render_bones )
		{
			return false;
		}

		const auto write_count = std::min( count, *obj_bone_count );
		for ( auto i = 0; i < write_count; ++i )
		{
			const auto& b = bones[ i ];
			const auto dst = *render_bones + ( static_cast< std::size_t >( i ) * 48 );

			const auto bxx = b.rotation.x * b.rotation.x;
			const auto byy = b.rotation.y * b.rotation.y;
			const auto bzz = b.rotation.z * b.rotation.z;
			const auto bxy = b.rotation.x * b.rotation.y;
			const auto bxz = b.rotation.x * b.rotation.z;
			const auto byz = b.rotation.y * b.rotation.z;
			const auto bwx = b.rotation.w * b.rotation.x;
			const auto bwy = b.rotation.w * b.rotation.y;
			const auto bwz = b.rotation.w * b.rotation.z;

			const bone_matrix matrix{
				1.0f - 2.0f * ( byy + bzz ), 2.0f * ( bxy - bwz ), 2.0f * ( bxz + bwy ), b.position.x,
				2.0f * ( bxy + bwz ), 1.0f - 2.0f * ( bxx + bzz ), 2.0f * ( byz - bwx ), b.position.y,
				2.0f * ( bxz - bwy ), 2.0f * ( byz + bwx ), 1.0f - 2.0f * ( bxx + byy ), b.position.z
			};

			if ( !memory::safe_write( dst, matrix ) )
			{
				return false;
			}
		}

		return true;
	}

	void chams::onshot::push( std::uintptr_t pawn )
	{
		const auto& cfg = settings::g_esp.m_player.m_chams;
		if ( !cfg.onshot.enabled.value || !memory::is_game_ptr( pawn ) || !systems::g_local.get( ).is_alive )
		{
			return;
		}

		const auto node_off = SCHEMA_OFFSET( "C_BaseEntity", "m_pGameSceneNode"_hash );
		const auto model_state_off = SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash );
		const auto node = node_off ? memory::safe_read<std::uintptr_t>( pawn + node_off ) : std::nullopt;
		if ( node && memory::is_game_ptr( *node ) && model_state_off )
		{
			const auto game_scene_node = *node;
			const auto bone_cache = memory::safe_read<std::uintptr_t>( game_scene_node + model_state_off + 0x80 );
			const auto bone_count_opt = memory::safe_read<int>( game_scene_node + model_state_off + 0x8c );
			if ( bone_cache && memory::is_game_ptr( *bone_cache ) && bone_count_opt && *bone_count_opt > 0 )
			{
				const auto bone_count = std::min( *bone_count_opt, k_max_bones );
				this->push( pawn, reinterpret_cast< const systems::bones::data* >( *bone_cache ), bone_count );
				return;
			}
		}

		const auto records = combat::g_shared.lc( ).get_valid_records( pawn );
		if ( !records.empty( ) )
		{
			const auto& record = records.front( );
			if ( record.bone_count > 0 )
			{
				this->push( pawn, record.bones, record.bone_count );
			}
		}
	}

	void chams::onshot::push( std::uintptr_t pawn, const systems::bones::data* bones, int bone_count )
	{
		const auto& cfg = settings::g_esp.m_player.m_chams;
		if ( !cfg.onshot.enabled.value || !memory::is_game_ptr( pawn ) || !bones || bone_count <= 0
			|| !systems::g_local.get( ).is_alive )
		{
			return;
		}

		const auto count = std::clamp( bone_count, 0, k_max_bones );

		std::lock_guard<std::mutex> lock( this->m_mutex );

		auto& pending = this->m_pending[ pawn ];
		pending.bone_count = count;
		std::copy_n( bones, count, pending.bones.begin( ) );
	}

	void chams::onshot::defer_destroy( backtrack::object& obj )
	{
		if ( obj.scene_object )
		{
			this->m_deferred_destroy.push_back( obj.scene_object );
		}

		obj.scene_object = 0;
		obj.active = false;
		obj.generation = 0;
		obj.applied_tick = -1;
	}

	void chams::onshot::flush_deferred( )
	{
		if ( this->m_destroy_delay > 0 )
		{
			--this->m_destroy_delay;
			return;
		}

		std::vector<std::uintptr_t> dying;
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			dying = this->m_deferred_destroy;
		}

		for ( const auto scene_object : dying )
		{
			destroy_scene_object( scene_object, "chams.os" );
		}

		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			for ( const auto scene_object : dying )
			{
				for ( auto it = this->m_deferred_destroy.begin( ); it != this->m_deferred_destroy.end( ); ++it )
				{
					if ( *it == scene_object )
					{
						this->m_deferred_destroy.erase( it );
						break;
					}
				}
			}
		}
	}

	void chams::onshot::update( )
	{
		const auto& cfg = settings::g_esp.m_player.m_chams;
		const auto local = systems::g_local.get( );

		if ( !cfg.onshot.enabled.value || !local.is_alive )
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			for ( auto& [pawn, e] : this->m_entries )
			{
				this->defer_destroy( e.visual );
			}
			this->m_entries.clear( );
			this->m_pending.clear( );
			this->m_destroy_delay = 3;
			return;
		}

		this->flush_deferred( );

		const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );
		if ( !global_vars )
		{
			return;
		}

		const auto current_time = memory::read<float>( global_vars + 0x30 );

		std::vector<std::pair<std::uintptr_t, pose>> pending_copy;
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			pending_copy.assign( this->m_pending.begin( ), this->m_pending.end( ) );
			this->m_pending.clear( );
		}

		for ( auto& [pawn, pending] : pending_copy )
		{
			std::uintptr_t existing{};
			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto it = this->m_entries.find( pawn );
				if ( it != this->m_entries.end( ) )
				{
					existing = it->second.visual.scene_object;
					if ( existing && !scene_object_alive( existing ) )
					{
						this->defer_destroy( it->second.visual );
						existing = 0;
					}
				}
			}

			backtrack::object created{};
			if ( !existing )
			{
				created.create( pawn );
				if ( !created.scene_object )
				{
					std::lock_guard<std::mutex> lock( this->m_mutex );
					if ( !this->m_pending.contains( pawn ) )
					{
						this->m_pending[ pawn ] = pending;
					}
					continue;
				}
				existing = created.scene_object;
			}

			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto& e = this->m_entries[ pawn ];
				if ( !e.visual.scene_object )
				{
					e.visual.scene_object = existing;
					e.visual.pawn = pawn;
					e.visual.generation = 1;
				}
				e.shot_pose = pending;
				e.visual.spawn_time = current_time;
				e.visual.active = false;
				e.visual.applied_tick = -1;
				e.pose_applied = false;
				existing = e.visual.scene_object;
			}

			auto pose_applied = false;
			if ( existing && pending.bone_count > 0 )
			{
				backtrack::object tmp{};
				tmp.scene_object = existing;
				pose_applied = tmp.setup_bones( pending.bones.data( ), pending.bone_count );
			}

			{
				std::lock_guard<std::mutex> lock( this->m_mutex );
				auto it = this->m_entries.find( pawn );
				if ( it != this->m_entries.end( ) && it->second.visual.scene_object == existing )
				{
					it->second.pose_applied = pose_applied;
					it->second.visual.active = pose_applied;
				}
			}
		}

		std::vector<std::pair<std::uintptr_t, pose>> retries;
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			const auto fade_time = cfg.onshot_fade_time.value;

			for ( auto it = this->m_entries.begin( ); it != this->m_entries.end( ); )
			{
				const auto scene_object = it->second.visual.scene_object;
				if ( !scene_object || !scene_object_alive( scene_object ) )
				{
					this->defer_destroy( it->second.visual );
					it = this->m_entries.erase( it );
					continue;
				}

				if ( current_time - it->second.visual.spawn_time >= fade_time )
				{
					this->defer_destroy( it->second.visual );
					it = this->m_entries.erase( it );
					continue;
				}

				if ( !it->second.pose_applied && it->second.shot_pose.bone_count > 0 )
				{
					retries.emplace_back( scene_object, it->second.shot_pose );
				}

				++it;
			}
		}

		for ( auto& [scene_object, shot_pose] : retries )
		{
			backtrack::object tmp{};
			tmp.scene_object = scene_object;
			if ( !tmp.setup_bones( shot_pose.bones.data( ), shot_pose.bone_count ) )
			{
				continue;
			}

			std::lock_guard<std::mutex> lock( this->m_mutex );
			for ( auto& [pawn, e] : this->m_entries )
			{
				if ( e.visual.scene_object == scene_object )
				{
					e.pose_applied = true;
					e.visual.active = true;
					break;
				}
			}
		}
	}

	void chams::onshot::shutdown( bool destroy_objects )
	{
		std::vector<std::uintptr_t> dying;
		{
			std::lock_guard<std::mutex> lock( this->m_mutex );
			if ( destroy_objects )
			{
				for ( auto& [pawn, e] : this->m_entries )
				{
					if ( e.visual.scene_object )
					{
						dying.push_back( e.visual.scene_object );
						e.visual.scene_object = 0;
						e.visual.active = false;
					}
				}
				dying.insert( dying.end( ), this->m_deferred_destroy.begin( ), this->m_deferred_destroy.end( ) );
			}
			this->m_entries.clear( );
			this->m_pending.clear( );
			this->m_deferred_destroy.clear( );
		}

		if ( !destroy_objects )
			return;

		for ( const auto scene_object : dying )
		{
			destroy_scene_object( scene_object, "chams.os" );
		}
	}

	bool chams::onshot::has_active( std::uintptr_t pawn ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );

		auto it = this->m_entries.find( pawn );
		return it != this->m_entries.end( ) && it->second.visual.scene_object && it->second.visual.active;
	}

	std::uintptr_t chams::onshot::get_scene_object( std::uintptr_t pawn ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );

		auto it = this->m_entries.find( pawn );
		if ( it != this->m_entries.end( ) )
		{
			return it->second.visual.scene_object;
		}

		return 0;
	}

	float chams::onshot::get_alpha( std::uintptr_t pawn ) const
	{
		std::lock_guard<std::mutex> lock( this->m_mutex );

		auto it = this->m_entries.find( pawn );
		if ( it == this->m_entries.end( ) || !it->second.visual.scene_object || !it->second.visual.active )
		{
			return 0.0f;
		}

		const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );
		if ( !global_vars )
		{
			return 0.0f;
		}

		const auto current_time = memory::read<float>( global_vars + 0x30 );
		const auto fade_time = settings::g_esp.m_player.m_chams.onshot_fade_time.value;

		if ( fade_time <= 0.0f )
		{
			return 0.0f;
		}

		const auto elapsed = current_time - it->second.visual.spawn_time;
		if ( elapsed <= 0.0f )
		{
			return 1.0f;
		}

		const auto t = std::clamp( elapsed / fade_time, 0.0f, 1.0f );

		constexpr auto hold{ 0.2f };
		if ( t <= hold )
		{
			return 1.0f;
		}

		auto u = ( t - hold ) / ( 1.0f - hold );
		u = std::clamp( u, 0.0f, 1.0f );
		u = u * u * ( 3.0f - 2.0f * u );
		return 1.0f - u;
	}

	void chams::apply_onshot( std::uintptr_t pawn, std::uintptr_t os_obj, std::uintptr_t primitive_buffer, void( __fastcall* original_fn )( std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t ), std::uintptr_t a1, std::uintptr_t scene_view )
	{
		if ( !os_obj || !ghost_ready( os_obj ) || !this->m_onshot.has_active( pawn ) )
		{
			return;
		}

		const auto alpha = this->m_onshot.get_alpha( pawn );
		if ( alpha <= 0.01f )
		{
			return;
		}

		const auto fade = [ alpha ]( xdraw::color c ) -> xdraw::color
			{
				const auto scaled = static_cast< int >( std::lround( static_cast< float >( c.a ) * alpha ) );
				c.a = static_cast< std::uint8_t >( std::clamp( scaled, 0, 255 ) );
				return c;
			};

		auto faded_cfg = settings::g_esp.m_player.m_chams.onshot;
		faded_cfg.occluded_color.value = fade( faded_cfg.occluded_color.value );
		faded_cfg.visible_color.value = fade( faded_cfg.visible_color.value );

		if ( faded_cfg.overlay.enabled.value )
		{
			faded_cfg.overlay.color.value = fade( faded_cfg.overlay.color.value );
			faded_cfg.overlay.occluded_color.value = fade( faded_cfg.overlay.occluded_color.value );
		}

		const auto before = detail::read_primitive_buffer( primitive_buffer );
		const auto prev_count = before ? before->count( ) : -1;

		if ( faded_cfg.occluded_material.value != settings::esp::cham_ids::count )
		{
			this->apply_layer( primitive_buffer, original_fn, a1, os_obj, scene_view, faded_cfg.occluded_color, faded_cfg.occluded_material, true );
		}

		if ( faded_cfg.visible_material.value != settings::esp::cham_ids::count )
		{
			this->apply_layer( primitive_buffer, original_fn, a1, os_obj, scene_view, faded_cfg.visible_color, faded_cfg.visible_material, false );
		}

		if ( faded_cfg.overlay.enabled.value )
		{
			this->apply_overlay( primitive_buffer, original_fn, a1, os_obj, scene_view, faded_cfg.overlay.color, faded_cfg.overlay.material );
		}

		const auto after = detail::read_primitive_buffer( primitive_buffer );
		const auto new_count = after ? after->count( ) : -1;
		if ( after && prev_count >= 0 && new_count > prev_count )
		{
			for ( auto i = prev_count; i < new_count; ++i )
			{
				detail::mark_primitive_last( after->at( i ) );
			}
		}
	}

	void chams::apply_layer( std::uintptr_t primitive_buffer, void( __fastcall* original_fn )( std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t ), std::uintptr_t a1, std::uintptr_t scene_object, std::uintptr_t scene_view, const xdraw::color& color, settings::esp::cham_ids material_id, bool occluded )
	{
		if ( !scene_object_alive( scene_object ) )
		{
			return;
		}

		const auto before = detail::read_primitive_buffer( primitive_buffer );
		const auto prev_count = before ? before->count() : -1;

		original_fn( a1, scene_object, scene_view, primitive_buffer );

		const auto after = detail::read_primitive_buffer( primitive_buffer );
		const auto new_count = after ? after->count() : -1;
		if ( !after || prev_count < 0 || prev_count >= new_count )
		{
			return;
		}

		const auto material = systems::materials::find( material_id, occluded );
		if ( !material )
		{
			return;
		}

		for ( auto i = prev_count; i < new_count; ++i )
		{
			detail::replace_primitive( after->at( i ), material, color );
		}
	}

	void chams::apply_overlay( std::uintptr_t primitive_buffer, void( __fastcall* original_fn )( std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t ), std::uintptr_t a1, std::uintptr_t scene_object, std::uintptr_t scene_view, const xdraw::color& color, settings::esp::cham_ids material_id )
	{
		if ( !scene_object_alive( scene_object ) )
		{
			return;
		}

		const auto material = systems::materials::find( material_id );
		if ( !material )
		{
			return;
		}

		const auto before = detail::read_primitive_buffer( primitive_buffer );
		const auto prev_count = before ? before->count() : -1;

		original_fn( a1, scene_object, scene_view, primitive_buffer );

		const auto after = detail::read_primitive_buffer( primitive_buffer );
		const auto new_count = after ? after->count() : -1;
		if ( !after || prev_count < 0 || prev_count >= new_count )
		{
			return;
		}

		for ( auto i = prev_count; i < new_count; ++i )
		{
			detail::replace_primitive( after->at( i ), material, color );
		}

		this->add_overlay_material( material );
	}

	void chams::apply_clone( std::uintptr_t primitive_buffer, void( __fastcall* original_fn )( std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t ), std::uintptr_t a1, std::uintptr_t scene_object, std::uintptr_t scene_view, systems::materials::clone_type type )
	{
		if ( !scene_object_alive( scene_object ) )
		{
			return;
		}

		const auto before = detail::read_primitive_buffer( primitive_buffer );
		const auto prev_count = before ? before->count() : -1;

		original_fn( a1, scene_object, scene_view, primitive_buffer );

		const auto after = detail::read_primitive_buffer( primitive_buffer );
		const auto new_count = after ? after->count() : -1;
		if ( !after || prev_count < 0 || prev_count >= new_count )
		{
			return;
		}

		for ( auto i = prev_count; i < new_count; ++i )
		{
			const auto primitive = after->at( i );
			const auto orig_mat = memory::safe_read<std::uintptr_t>(
				primitive + detail::primitive_material_offset );

			if ( !orig_mat || !*orig_mat )
			{
				continue;
			}

			const auto clone = systems::materials::get_or_create_clone( *orig_mat, type );
			if ( !clone )
			{
				continue;
			}

			(void) memory::safe_write<std::uintptr_t>(
				primitive + detail::primitive_material_offset, clone );
			(void) memory::safe_write<std::uintptr_t>(
				primitive + detail::primitive_material_copy_offset, clone );
		}
	}

	bool chams::is_overlay_material( std::uintptr_t mat ) const
	{
		const auto count = this->m_overlay_material_count.load( std::memory_order_acquire );

		for ( auto i = 0; i < count; ++i )
		{
			if ( this->m_overlay_materials[ i ].load( std::memory_order_relaxed ) == mat )
			{
				return true;
			}
		}

		return false;
	}

	void chams::add_overlay_material( std::uintptr_t mat )
	{
		const auto count = this->m_overlay_material_count.load( std::memory_order_acquire );

		for ( auto i = 0; i < count; ++i )
		{
			if ( this->m_overlay_materials[ i ].load( std::memory_order_relaxed ) == mat )
			{
				return;
			}
		}

		const auto idx = this->m_overlay_material_count.fetch_add( 1, std::memory_order_acq_rel );

		if ( idx < k_max_overlay_materials )
		{
			this->m_overlay_materials[ idx ].store( mat, std::memory_order_release );
		}
		else
		{
			this->m_overlay_material_count.fetch_sub( 1, std::memory_order_release );
		}
	}

}
