#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/features.hpp>
#include <core/settings.hpp>
#include <modules/visuals/skinchanger/custom_paint.h>
namespace features::changer {

	void guns::on_frame_stage_notify( )
	{
		const auto local = systems::g_local.get( );
		if ( custom_paint::level_busy( ) )
		{
			custom_paint::note_spawned(
				custom_paint::session_playable( local.pawn )
				&& local.is_alive && local.controller
				&& !systems::g_local.is_in_cinematic( ) );
			return;
		}

		this->process_hud_clear( );

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

		const auto weapons_base = weapon_services + SCHEMA_OFFSET( "CPlayer_WeaponServices", "m_hMyWeapons"_hash );
		const auto weapons_size = memory::read<int>( weapons_base );
		const auto weapons_data = memory::read<std::uintptr_t>( weapons_base + 0x8 );

		if ( !weapons_data || weapons_size <= 0 )
		{
			return;
		}

		const auto steam_id = reinterpret_cast<CBasePlayerController*>( local.controller )->m_steamID();
		const auto account_id = static_cast< std::uint32_t >( steam_id & 0xffffffff );

		if ( this->m_tracked_pawn != local.pawn )
		{
			this->m_applied_weapons.clear( );
			this->m_originals.clear( );
			this->m_hud_synced.clear( );
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

			if ( econ_item_system::is_world_knife( static_cast< std::int16_t >( current_def_index ) ) )
			{
				continue;
			}

			if ( !current_def || current_def->category != econ_item_system::item_category::gun )
			{
				continue;
			}

			this->capture_original( weapon, iv, handle );

			const auto skin_it = settings::g_changer.skins.data.find( current_def_index );
			if ( skin_it == settings::g_changer.skins.data.end( ) )
			{
				this->restore( weapon, iv, handle );
				continue;
			}

			const auto& skin = skin_it->second;
			const auto current_pk = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit();
			const auto applied_it = this->m_applied_weapons.find( handle );
			const bool already = applied_it != this->m_applied_weapons.end( ) && applied_it->second == skin && current_pk == skin.paint_kit_id;
			const bool ready = custom_paint::entity_model_ready( weapon );
			const auto weapon_services_active = systems::g_entities.lookup( reinterpret_cast< CPlayer_WeaponServices* >( weapon_services )->m_hActiveWeapon( ) );
			const bool is_active = weapon_services_active == weapon;

			if ( already && ready )
			{
				this->apply_mesh( weapon, local.pawn, &skin );
				if ( is_active && !this->m_hud_synced.contains( handle ) )
				{
					this->rebuild_paint( weapon );
					this->m_hud_synced.insert( handle );
				}
				else if ( !is_active )
				{
					this->m_hud_synced.erase( handle );
				}
				continue;
			}

			this->apply( weapon, iv, &skin, account_id );
			if ( custom_paint::entity_model_ready( weapon ) )
				this->m_applied_weapons[ handle ] = skin;
			else
				this->m_applied_weapons.erase( handle );

			if ( is_active && custom_paint::entity_model_ready( weapon ) )
				this->m_hud_synced.insert( handle );
			else
				this->m_hud_synced.erase( handle );

			this->apply_mesh( weapon, local.pawn, &skin );
		}
	}

	void guns::apply( std::uintptr_t weapon, std::uintptr_t iv, const settings::changer::applied_skin* skin, std::uint32_t account_id )
	{
		this->m_pending_hud_iv = 0;

		custom_paint::write_item_identity( iv, account_id, custom_paint::bump_item_serial( ) );

		auto* econ = reinterpret_cast< C_EconEntity* >( weapon );
		econ->m_bAttributesInitialized( ) = true;
		econ->m_nFallbackPaintKit( ) = skin->paint_kit_id;
		econ->m_nFallbackSeed( ) = ( std::max )( 1, skin->seed );
		econ->m_flFallbackWear( ) = skin->wear;
		econ->m_nFallbackStatTrak( ) = skin->stattrak ? 0 : -1;
		custom_paint::write_paint_attributes( iv, skin->paint_kit_id, ( std::max )( 1, skin->seed ), skin->wear );

		if ( skin->use_custom_colors )
			custom_paint::prepare_kit( skin->paint_kit_id, skin->custom_colors, true );
		else
			custom_paint::restore_kit_colors( skin->paint_kit_id );

		this->rebuild_paint( weapon );
		this->apply_mesh( weapon, this->m_tracked_pawn, skin );
		this->schedule_hud_clear( iv );
	}

	void guns::capture_original( std::uintptr_t weapon, std::uintptr_t iv, std::uint32_t handle )
	{
		auto& original = this->m_originals[ handle ];
		if ( original.captured || this->m_applied_weapons.contains( handle ) )
			return;

		original.def_index = reinterpret_cast<C_EconItemView*>( iv )->m_iItemDefinitionIndex();
		original.id_high = reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDHigh();
		original.id_low = reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDLow();
		original.account_id = reinterpret_cast<C_EconItemView*>( iv )->m_iAccountID();
		original.initialized = reinterpret_cast<C_EconItemView*>( iv )->m_bInitialized();
		original.paint_kit = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit();
		original.seed = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackSeed();
		original.wear = reinterpret_cast<C_EconEntity*>( weapon )->m_flFallbackWear();
		original.stattrak = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackStatTrak();
		original.captured = true;
	}

	void guns::restore( std::uintptr_t weapon, std::uintptr_t iv, std::uint32_t handle )
	{
		const auto it = this->m_originals.find( handle );
		if ( it == this->m_originals.end( ) || !it->second.captured )
			return;

		const auto& original = it->second;
		const auto current_pk = reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit();
		const auto current_high = reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDHigh();
		if ( !this->m_applied_weapons.contains( handle ) && current_pk == original.paint_kit && current_high == original.id_high )
			return;
		reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDHigh() = original.id_high;
		reinterpret_cast<C_EconItemView*>( iv )->m_iItemIDLow() = original.id_low;
		reinterpret_cast<C_EconItemView*>( iv )->m_iItemID() = ( static_cast< std::uint64_t >( original.id_high ) << 32 ) | original.id_low;
		reinterpret_cast<C_EconItemView*>( iv )->m_iAccountID() = original.account_id;
		reinterpret_cast<C_EconItemView*>( iv )->m_bInitialized() = original.initialized;

		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackPaintKit() = original.paint_kit;
		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackSeed() = original.seed;
		reinterpret_cast<C_EconEntity*>( weapon )->m_flFallbackWear() = original.wear;
		reinterpret_cast<C_EconEntity*>( weapon )->m_nFallbackStatTrak() = original.stattrak;
		custom_paint::write_paint_attributes( iv, original.paint_kit, original.seed, original.wear );
		custom_paint::restore_kit_colors( original.paint_kit );

		this->rebuild_paint( weapon );
		this->apply_mesh_kit( weapon, this->m_tracked_pawn, original.paint_kit );
		this->schedule_hud_clear( iv );
		this->m_applied_weapons.erase( handle );
		this->m_hud_synced.erase( handle );
	}

	void guns::rebuild_paint( std::uintptr_t weapon )
	{
		custom_paint::refresh_weapon_visuals( weapon, this->m_tracked_pawn );
	}

	void guns::apply_mesh( std::uintptr_t weapon, std::uintptr_t pawn, const settings::changer::applied_skin* skin ) const
	{
		if ( !weapon || !skin )
			return;

		const auto pk = g_econ_item_system.find_paint_kit( skin->paint_kit_id );
		std::uintptr_t hud_pawn{};
		if ( pawn )
		{
			const auto weapon_services = reinterpret_cast< C_BasePlayerPawn* >( pawn )->m_pWeaponServices( );
			if ( weapon_services )
			{
				const auto active = systems::g_entities.lookup( reinterpret_cast< CPlayer_WeaponServices* >( weapon_services )->m_hActiveWeapon( ) );
				if ( active == weapon )
					hud_pawn = pawn;
			}
		}

		custom_paint::apply_weapon_mesh_mask( weapon, hud_pawn, pk && pk->legacy_model );
	}

	void guns::apply_mesh_kit( std::uintptr_t weapon, std::uintptr_t pawn, int paint_kit_id ) const
	{
		settings::changer::applied_skin skin{};
		skin.paint_kit_id = paint_kit_id;
		this->apply_mesh( weapon, pawn, &skin );
	}

	void guns::clear_hud_icon( std::uintptr_t iv )
	{
		const auto invalidate = PATTERN(PATTERN_ECON_ITEM_VIEW_INVALIDATE_DESCRIPTION);
		if ( iv && invalidate )
		{
			memory::call<void>( invalidate, iv );
		}
	}

	void guns::schedule_hud_clear( std::uintptr_t iv )
	{
		this->clear_hud_icon( iv );
		this->m_pending_hud_iv = iv;
	}

	void guns::process_hud_clear( )
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

	void guns::on_level_end( )
	{
		this->m_applied_weapons.clear( );
		this->m_originals.clear( );
		this->m_hud_synced.clear( );
		this->m_tracked_pawn = 0;
		this->m_pending_hud_iv = 0;
	}

	void guns::invalidate( )
	{
		this->m_applied_weapons.clear( );
		this->m_hud_synced.clear( );
	}

}



