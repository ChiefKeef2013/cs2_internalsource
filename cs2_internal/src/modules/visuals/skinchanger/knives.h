#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/settings.hpp>
#include <modules/visuals/skinchanger/custom_paint.h>
namespace features::changer {

	namespace detail {

		inline static std::uint32_t murmurhash2_lower( const char* str, int len, std::uint32_t seed )
		{
			constexpr auto m{ 0x5bd1e995 };
			constexpr auto r{ 24 };

			auto h = seed ^ len;
			auto i{ 0 };

			while ( len >= 4 )
			{
				auto k =
					static_cast< std::uint32_t >( ( str[ i ] >= 'A' && str[ i ] <= 'Z' ) ? str[ i ] + 32 : str[ i ] ) |
					( static_cast< std::uint32_t >( ( str[ i + 1 ] >= 'A' && str[ i + 1 ] <= 'Z' ) ? str[ i + 1 ] + 32 : str[ i + 1 ] ) << 8 ) |
					( static_cast< std::uint32_t >( ( str[ i + 2 ] >= 'A' && str[ i + 2 ] <= 'Z' ) ? str[ i + 2 ] + 32 : str[ i + 2 ] ) << 16 ) |
					( static_cast< std::uint32_t >( ( str[ i + 3 ] >= 'A' && str[ i + 3 ] <= 'Z' ) ? str[ i + 3 ] + 32 : str[ i + 3 ] ) << 24 );

				k *= m;
				k ^= k >> r;
				k *= m;

				h *= m;
				h ^= k;

				i += 4;
				len -= 4;
			}

			switch ( len )
			{
			case 3: h ^= static_cast< std::uint32_t >( ( str[ i + 2 ] >= 'A' && str[ i + 2 ] <= 'Z' ) ? str[ i + 2 ] + 32 : str[ i + 2 ] ) << 16; [[fallthrough]];
			case 2: h ^= static_cast< std::uint32_t >( ( str[ i + 1 ] >= 'A' && str[ i + 1 ] <= 'Z' ) ? str[ i + 1 ] + 32 : str[ i + 1 ] ) << 8; [[fallthrough]];
			case 1: h ^= static_cast< std::uint32_t >( ( str[ i ] >= 'A' && str[ i ] <= 'Z' ) ? str[ i ] + 32 : str[ i ] ); h *= m;
			}

			h ^= h >> 13;
			h *= m;
			h ^= h >> 15;

			return h;
		}

		inline static std::uint32_t make_subclass_token( const char* str )
		{
			if ( !str || !*str )
				return 0;

			return murmurhash2_lower( str, static_cast< int >( std::strlen( str ) ), 0x31415926 );
		}

		inline static std::uint32_t make_subclass_token( std::int16_t def_index )
		{
			const auto s = std::to_string( def_index );
			return make_subclass_token( s.c_str( ) );
		}

		inline static const char* model_path_for( const econ_item_system::item_def* def, std::uintptr_t iv )
		{
			if ( def && !def->model_player.empty( ) )
				return def->model_player.c_str( );

			if ( const auto fn = PATTERN( PATTERN_WEAPON_GET_MODEL_PATH ) )
				return memory::call<const char*>( fn, iv );

			return nullptr;
		}

	}

	void knives::on_frame_stage_notify( )
	{
		this->process_hud_clear( );

		const auto local = systems::g_local.get( );
		if ( custom_paint::level_busy( ) )
		{
			custom_paint::note_spawned(
				custom_paint::session_playable( local.pawn )
				&& local.is_alive && local.controller
				&& !systems::g_local.is_in_cinematic( ) );
			return;
		}

		if ( !custom_paint::session_playable( local.pawn ) )
			return;

		if ( !local.is_alive || systems::g_local.is_in_cinematic( ) || !local.pawn || !local.controller )
		{
			return;
		}

		const auto weapon_services = reinterpret_cast<C_BasePlayerPawn*>( local.pawn )->m_pWeaponServices();
		if ( !weapon_services )
		{
			return;
		}

		const settings::changer::applied_skin* selected_skin{ nullptr };
		const econ_item_system::item_def* selected_knife_def{ nullptr };

		for ( const auto& [def_idx, skin] : settings::g_changer.skins.data )
		{
			const auto def = g_econ_item_system.find_def( def_idx );
			if ( !def || ( def->category != econ_item_system::item_category::knife && !econ_item_system::is_changeable_knife( def_idx ) ) )
			{
				continue;
			}

			selected_skin = &skin;
			selected_knife_def = def;
			break;
		}

		const auto weapons_base = weapon_services + SCHEMA_OFFSET( "CPlayer_WeaponServices", "m_hMyWeapons"_hash );
		const auto weapons_size = memory::read<int>( weapons_base );
		const auto weapons_data = memory::read<std::uintptr_t>( weapons_base + 0x8 );

		if ( !weapons_data || weapons_size <= 0 )
		{
			return;
		}

		const auto active_handle = reinterpret_cast<CPlayer_WeaponServices*>( weapon_services )->m_hActiveWeapon();
		const auto active_weapon = systems::g_entities.lookup( active_handle );

		if ( this->m_tracked_pawn != local.pawn )
		{
			this->m_original = {};
			this->m_overridden = false;
			this->m_paint_rebuilt = false;
			this->m_applied = {};
			this->m_last_active_handle = 0;
			this->m_applied_model.clear( );
			this->m_tracked_pawn = local.pawn;
		}

		for ( auto i = 0; i < weapons_size; ++i )
		{
			const auto handle = memory::read<std::uint32_t>( weapons_data + i * sizeof( std::uint32_t ) );
			const auto weapon = systems::g_entities.lookup( handle );

			if ( !weapon )
			{
				continue;
			}

			const auto iv = reinterpret_cast<std::uintptr_t>( &reinterpret_cast<C_EconEntity*>( weapon )->m_AttributeManager().m_Item() );
			const auto current_def_index = reinterpret_cast<C_EconItemView*>( iv )->m_iItemDefinitionIndex();
			const auto current_def = g_econ_item_system.find_def( static_cast< std::int16_t >( current_def_index ) );

			if ( !econ_item_system::is_world_knife( static_cast< std::int16_t >( current_def_index ) )
				&& ( !current_def || current_def->category != econ_item_system::item_category::knife ) )
			{
				continue;
			}

			if ( !memory::read<std::uintptr_t>( weapon + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 ) && !this->m_overridden )
			{
				continue;
			}

			if ( !selected_knife_def )
			{
				this->restore( weapon, iv, active_weapon, local.pawn );
				this->finish_hud_restore( local.pawn );
				break;
			}

			if ( this->m_last_selected_def != selected_knife_def->def_index || this->m_applied != *selected_skin )
			{
				this->m_overridden = false;
				this->m_paint_rebuilt = false;
				this->m_last_selected_def = selected_knife_def->def_index;
				this->m_applied = *selected_skin;
			}

			if ( !this->m_overridden )
			{
				this->capture_original( weapon, iv );
			}

			const auto target_token = detail::make_subclass_token( selected_knife_def->def_index );
			const auto class_token = detail::make_subclass_token( selected_knife_def->item_class.c_str( ) );
			const auto current_subclass = reinterpret_cast<C_BaseEntity*>( weapon )->m_nSubclassID();
			const auto current_pk = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit();
			const auto model_path = detail::model_path_for( selected_knife_def, iv );
			const bool subclass_ok = current_subclass == target_token || ( class_token && current_subclass == class_token );

			if ( subclass_ok
				&& current_pk == selected_skin->paint_kit_id
				&& this->m_overridden
				&& this->m_paint_rebuilt
				&& custom_paint::knife_models_ready( weapon, local.pawn, model_path, weapon == active_weapon ) )
			{
				const auto pk = g_econ_item_system.find_paint_kit( selected_skin->paint_kit_id );
				custom_paint::apply_weapon_mesh_mask(
					weapon,
					weapon == active_weapon ? local.pawn : 0,
					pk && pk->legacy_model );
				break;
			}

			const auto steam_id = reinterpret_cast<CBasePlayerController*>( local.controller )->m_steamID();
			const auto account_id = static_cast< std::uint32_t >( steam_id & 0xffffffff );

			this->apply( weapon, iv, selected_knife_def, selected_skin, account_id, active_weapon, local.pawn );
			break;
		}

		if ( active_handle != this->m_last_active_handle )
		{
			this->m_last_active_handle = active_handle;

			if ( this->m_overridden && active_weapon )
			{
				const auto iv = reinterpret_cast<std::uintptr_t>( &reinterpret_cast<C_EconEntity*>( active_weapon )->m_AttributeManager().m_Item() );
				const auto def_index = reinterpret_cast<C_EconItemView*>( iv )->m_iItemDefinitionIndex();
				const auto def = g_econ_item_system.find_def( static_cast< std::int16_t >( def_index ) );

				if ( def && def->category == econ_item_system::item_category::knife )
				{
					const auto paint_kit_id = reinterpret_cast<C_EconEntity*>( active_weapon )->m_nFallbackPaintKit();
					const auto pk = g_econ_item_system.find_paint_kit( paint_kit_id );
					this->update_view_model( local.pawn, pk );
				}
			}
		}
	}

	void knives::capture_original( std::uintptr_t weapon, std::uintptr_t iv )
	{
		if ( this->m_original.captured )
		{
			return;
		}

		this->m_original.def_index = reinterpret_cast<C_EconItemView*>( iv )->m_iItemDefinitionIndex();
		this->m_original.id_high = reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDHigh();
		this->m_original.id_low = reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDLow();
		this->m_original.account_id = reinterpret_cast<C_EconItemView*>( iv )->m_iAccountID();
		this->m_original.initialized = reinterpret_cast<C_EconItemView*>( iv )->m_bInitialized();
		this->m_original.paint_kit = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit();
		this->m_original.seed = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackSeed();
		this->m_original.wear = reinterpret_cast<C_EconEntity*>( weapon )->m_flFallbackWear();
		this->m_original.stattrak = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackStatTrak();
		this->m_original.model = custom_paint::entity_model_path( weapon );
		this->m_original.captured = true;
	}

	void knives::apply( std::uintptr_t weapon, std::uintptr_t iv, const econ_item_system::item_def* def, const settings::changer::applied_skin* skin, std::uint32_t account_id, std::uintptr_t active_weapon, std::uintptr_t pawn )
	{
		this->m_pending_hud_iv = 0;
		this->m_restore_hud_lookup.clear( );

		reinterpret_cast<C_EconItemView*>( iv )->m_iItemDefinitionIndex() = static_cast< std::uint16_t >( def->def_index );
		custom_paint::write_item_identity( iv, account_id, custom_paint::bump_item_serial( ) );
		reinterpret_cast<C_EconItemView*>( iv )->m_iEntityQuality() = 3;

		auto* econ = reinterpret_cast<C_EconEntity*>( weapon );
		econ->m_bAttributesInitialized() = true;
		econ->m_nFallbackPaintKit() = skin->paint_kit_id;
		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackSeed() = ( std::max )( 1, skin->seed );
		reinterpret_cast<C_EconEntity*>( weapon )->m_flFallbackWear() = skin->wear;
		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackStatTrak() = skin->stattrak ? 0 : -1;
		custom_paint::write_paint_attributes( iv, skin->paint_kit_id, ( std::max )( 1, skin->seed ), skin->wear );

		if ( skin->use_custom_colors && !custom_paint::level_busy( ) )
			custom_paint::prepare_kit( skin->paint_kit_id, skin->custom_colors, true );
		else if ( !skin->use_custom_colors )
			custom_paint::restore_kit_colors( skin->paint_kit_id );

		const auto pk = g_econ_item_system.find_paint_kit( skin->paint_kit_id );

		if ( !custom_paint::level_busy( ) )
		{
			this->update_model( weapon, iv, static_cast< std::uint16_t >( def->def_index ), pawn, weapon == active_weapon );
			this->rebuild_paint( weapon, active_weapon, pawn, pk );
			this->m_paint_rebuilt = custom_paint::knife_models_ready( weapon, pawn, detail::model_path_for( def, iv ), weapon == active_weapon );
		}
		else
		{
			this->m_paint_rebuilt = false;
		}
		this->schedule_hud_clear( iv );

		this->m_overridden = true;
	}

	void knives::restore( std::uintptr_t weapon, std::uintptr_t iv, std::uintptr_t active_weapon, std::uintptr_t pawn )
	{
		if ( !this->m_overridden || !this->m_original.captured )
		{
			return;
		}

		this->m_pending_hud_iv = 0;

		const auto hud_lookup = this->m_applied_model;
		this->m_restore_hud_lookup = hud_lookup;

		reinterpret_cast<C_EconItemView*>( iv )->m_iItemDefinitionIndex() = this->m_original.def_index;
		reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDHigh() = this->m_original.id_high;
		reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDLow() = this->m_original.id_low;
		reinterpret_cast<C_EconItemView*>( iv )->m_iItemID() = ( static_cast< std::uint64_t >( this->m_original.id_high ) << 32 ) | this->m_original.id_low;
		reinterpret_cast<C_EconItemView*>( iv )->m_iAccountID() = this->m_original.account_id;
		reinterpret_cast<C_EconItemView*>( iv )->m_bInitialized() = this->m_original.initialized;

		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit() = this->m_original.paint_kit;
		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackSeed() = this->m_original.seed;
		reinterpret_cast<C_EconEntity*>( weapon )->m_flFallbackWear() = this->m_original.wear;
		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackStatTrak() = this->m_original.stattrak;

		const auto pk = g_econ_item_system.find_paint_kit( this->m_original.paint_kit );

		this->update_model( weapon, iv, this->m_original.def_index, pawn, true, hud_lookup.empty( ) ? nullptr : hud_lookup.c_str( ) );
		this->rebuild_paint( weapon, active_weapon, pawn, pk );
		this->schedule_hud_clear( iv );

		this->m_overridden = false;
		this->m_paint_rebuilt = false;
		this->m_applied = {};
		this->m_last_selected_def = 0;
		this->m_applied_model = this->m_original.model;
	}

	void knives::finish_hud_restore( std::uintptr_t pawn )
	{
		if ( this->m_restore_hud_lookup.empty( ) )
			return;

		const char* original_path = this->m_original.model.empty( ) ? nullptr : this->m_original.model.c_str( );
		if ( !original_path )
		{
			const auto def = g_econ_item_system.find_def( static_cast< std::int16_t >( this->m_original.def_index ) );
			if ( def && !def->model_player.empty( ) )
				original_path = def->model_player.c_str( );
		}
		if ( !original_path )
			return;

		std::uintptr_t hud = custom_paint::find_hud_weapon_for_path( pawn, this->m_restore_hud_lookup.c_str( ) );
		if ( !hud )
			hud = custom_paint::find_hud_weapon_for_path( pawn, original_path );

		if ( !hud )
			return;

		custom_paint::apply_hud_model( hud, original_path );
		if ( custom_paint::entity_uses_model( hud, original_path ) )
			this->m_restore_hud_lookup.clear( );
	}

	void knives::update_model( std::uintptr_t weapon, std::uintptr_t iv, std::uint16_t def_index, std::uintptr_t pawn, bool apply_hud, const char* hud_lookup )
	{
		const auto def = g_econ_item_system.find_def( static_cast< std::int16_t >( def_index ) );
		auto token = detail::make_subclass_token( static_cast< std::int16_t >( def_index ) );

		reinterpret_cast<C_BaseEntity*>( weapon )->m_nSubclassID() = token;
		if ( const auto update_vdata = PATTERN( PATTERN_WEAPON_GET_VIEWMODEL ) )
			memory::call<void>( update_vdata, weapon );

		const auto subclass_off = SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash );
		const auto vdata = subclass_off ? memory::read<std::uintptr_t>( weapon + subclass_off + 0x8 ) : 0;
		if ( !vdata && def && !def->item_class.empty( ) )
		{
			token = detail::make_subclass_token( def->item_class.c_str( ) );
			if ( token )
			{
				reinterpret_cast<C_BaseEntity*>( weapon )->m_nSubclassID() = token;
				if ( const auto update_vdata = PATTERN( PATTERN_WEAPON_GET_VIEWMODEL ) )
					memory::call<void>( update_vdata, weapon );
			}
		}

		const auto model_path = detail::model_path_for( def, iv );
		if ( model_path && *model_path )
		{
			this->m_applied_model = model_path;
			custom_paint::apply_knife_models( weapon, pawn, model_path, apply_hud, hud_lookup );
		}
		else
		{
			this->m_applied_model.clear( );
		}
	}

	void knives::rebuild_paint( std::uintptr_t weapon, std::uintptr_t active_weapon, std::uintptr_t pawn, const econ_item_system::paint_kit* pk )
	{
		custom_paint::refresh_weapon_visuals( weapon, pawn );

		const auto is_legacy = pk && pk->legacy_model;
		custom_paint::apply_weapon_mesh_mask( weapon, weapon == active_weapon ? pawn : 0, is_legacy );

		if ( weapon == active_weapon )
		{
			this->update_view_model( pawn, pk );
		}
	}

	void knives::update_view_model( std::uintptr_t pawn, const econ_item_system::paint_kit* pk )
	{
		const auto view_model = this->find_hud_model_weapon( pawn );
		if ( !view_model )
			return;

		if ( !this->m_applied_model.empty( ) )
			custom_paint::apply_hud_model( view_model, this->m_applied_model.c_str( ) );

		custom_paint::set_mesh_group_mask( view_model, pk && pk->legacy_model ? 2ull : 1ull );
	}

	std::uintptr_t knives::find_hud_model_weapon( std::uintptr_t pawn )
	{
		if ( this->m_applied_model.empty( ) )
			return custom_paint::find_hud_weapon( pawn );
		return custom_paint::find_hud_weapon_for_path( pawn, this->m_applied_model.c_str( ) );
	}

	void knives::clear_hud_icon( std::uintptr_t iv )
	{
		const auto invalidate = PATTERN(PATTERN_ECON_ITEM_VIEW_INVALIDATE_DESCRIPTION);
		if ( iv && invalidate )
		{
			memory::call<void>( invalidate, iv );
		}
	}

	void knives::schedule_hud_clear( std::uintptr_t iv )
	{
		this->clear_hud_icon( iv );
		this->m_pending_hud_iv = iv;
	}

	void knives::process_hud_clear( )
	{
		if ( !this->m_pending_hud_iv )
		{
			return;
		}

		this->m_pending_hud_iv = 0;

		const auto refresh = PATTERN(PATTERN_HUD_WEAPON_SELECTION_REFRESH);
		if ( refresh )
		{
			memory::call<void>( refresh );
		}
	}

	void knives::on_level_end( )
	{
		this->m_original = {};
		this->m_tracked_pawn = 0;
		this->m_last_active_handle = 0;
		this->m_last_selected_def = 0;
		this->m_applied = {};
		this->m_overridden = false;
		this->m_paint_rebuilt = false;
		this->m_pending_hud_iv = 0;
		this->m_applied_model.clear( );
		this->m_restore_hud_lookup.clear( );
	}

	void knives::invalidate( )
	{
		this->m_paint_rebuilt = false;
		this->m_applied = {};
		this->m_last_selected_def = 0;
	}

}



