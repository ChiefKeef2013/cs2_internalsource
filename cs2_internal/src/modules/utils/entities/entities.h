#pragma once

#include <core/common.hpp>
#include <core/memory.hpp>

#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerController.h>
#include <valve/schemas/CBasePlayerPawn.h>
#include <valve/schemas/CCollisionProperty.h>
#include <valve/schemas/CCSGameRules.h>
#include <valve/schemas/CEconEntity.h>
#include <valve/schemas/CGameSceneNode.h>
#include <valve/schemas/CGrenade.h>
#include <valve/schemas/CPlantedC4.h>
#include <valve/schemas/CWeapon.h>

namespace systems {

	void entities::on_add_entity( std::uintptr_t entity, std::uint32_t handle )
	{
		if ( !entity )
		{
			return;
		}

		const auto index = static_cast< std::int16_t >( handle & 0x7fff );
		if ( index < 0 || index > 0x3fff )
		{
			return;
		}

		const auto schema_name = this->get_schema_name( entity );
		if ( !schema_name || !schema_name[ 0 ] )
		{
			return;
		}

		const auto hashed = fnv1a::runtime_hash( schema_name );
		const auto entity_type = this->classify_entity( hashed );

		if ( entity_type == type::unknown )
		{
			return;
		}

		cached entry{};
		entry.ptr = entity;
		entry.index = index;
		entry.schema_hash = hashed;
		entry.type = entity_type;

		std::unique_lock lock( this->m_cache_mtx );

		for ( auto& c : this->m_cached )
		{
			if ( c.index == index )
			{
				c = entry;
				return;
			}
		}

		this->m_cached.emplace_back( entry );
	}

	void entities::on_remove_entity( std::uintptr_t entity, std::uint32_t handle )
	{
		if ( !entity )
		{
			return;
		}

		const auto index = static_cast< std::int16_t >( handle & 0x7fff );
		if ( index < 0 || index > 0x3fff )
		{
			return;
		}

		std::unique_lock lock( this->m_cache_mtx );

		for ( auto it = this->m_cached.begin( ); it != this->m_cached.end( ); ++it )
		{
			if ( it->index == index )
			{
				if ( it != this->m_cached.end( ) - 1 )
				{
					*it = this->m_cached.back( );
				}

				this->m_cached.pop_back( );
				return;
			}
		}
	}

	void entities::force_update( )
	{
		{
			std::unique_lock lock( this->m_cache_mtx );
			this->m_cached.clear( );
			this->m_cached.reserve( 128 );
		}

		for ( auto i = 0; i < 2048; ++i )
		{
			const auto entity = this->get_by_index( i );
			if ( !entity )
			{
				continue;
			}

			this->on_add_entity( entity, static_cast< std::uint32_t >( i ) );
		}
	}

	void entities::clear( )
	{
		std::unique_lock lock( this->m_cache_mtx );
		this->m_cached.clear( );
		this->m_cached_list_entries.fill( 0 );
		this->m_cached_entity_list = 0;
	}

	bool entities::exists( std::uintptr_t entity_ptr ) const
	{
		std::shared_lock lock( this->m_cache_mtx );

		for ( const auto& c : this->m_cached )
		{
			if ( c.ptr == entity_ptr )
			{
				return true;
			}
		}

		return false;
	}

	const char* entities::get_schema_name( std::uintptr_t entity ) const
	{
		if ( !memory::is_game_ptr( entity ) )
		{
			return nullptr;
		}

		const auto identity = memory::read<std::uintptr_t>( entity + 0x10 );
		if ( !memory::is_game_ptr( identity ) )
		{
			return nullptr;
		}

		const auto entity_class = memory::read<std::uintptr_t>( identity + 0x8 );
		if ( !memory::is_game_ptr( entity_class ) )
		{
			return nullptr;
		}

		const auto class_info = memory::read<std::uintptr_t>( entity_class + 0x58 );
		if ( !memory::is_game_ptr( class_info ) )
		{
			return nullptr;
		}

		const auto name = memory::read<std::uintptr_t>( class_info + 0x8 );
		if ( !memory::detail::is_user_addr( name ) )
		{
			return nullptr;
		}

		return reinterpret_cast< const char* >( name );
	}

	std::uintptr_t entities::get_by_index( std::int32_t index )
	{
		const auto entity_list = memory::read<std::uintptr_t>( addresses::globals::entity_list );
		if ( !entity_list )
		{
			return 0;
		}

		if ( this->m_cached_entity_list != entity_list )
		{
			this->m_cached_entity_list = entity_list;
			this->m_cached_list_entries.fill( 0 );
		}

		if ( index < 0 )
		{
			return 0;
		}

		const auto chunk_index = index >> 9;
		if ( chunk_index < 0 || chunk_index >= static_cast< std::int32_t >( this->m_cached_list_entries.size( ) ) )
		{
			return 0;
		}

		if ( !this->m_cached_list_entries[ chunk_index ] )
		{
			this->m_cached_list_entries[ chunk_index ] = memory::read<std::uintptr_t>( entity_list + ( static_cast< std::uintptr_t >( chunk_index ) * 8 ) + 0x10 );
		}

		const auto list_entry = this->m_cached_list_entries[ chunk_index ];
		if ( !memory::is_game_ptr( list_entry ) )
		{
			return 0;
		}

		const auto entity = memory::read<std::uintptr_t>( list_entry + ( static_cast< std::uintptr_t >( index & 0x1ff ) * 112 ) );
		if ( !memory::is_game_ptr( entity ) )
		{
			return 0;
		}

		return entity;
	}

	std::uintptr_t entities::lookup( std::uint32_t handle ) const
	{
		if ( !handle || handle == 0xffffffffu || handle == 0xfffffffeu )
		{
			return 0;
		}

		const auto index = handle & 0x7fffu;
		if ( index >= 0x7ffeu )
		{
			return 0;
		}

		const auto entity_list = memory::read<std::uintptr_t>( addresses::globals::entity_list );
		if ( !entity_list )
		{
			return 0;
		}

		if ( this->m_cached_entity_list != entity_list )
		{
			this->m_cached_entity_list = entity_list;
			this->m_cached_list_entries.fill( 0 );
		}

		const auto chunk_index = index >> 9;
		if ( chunk_index >= 64u )
		{
			return 0;
		}

		if ( !this->m_cached_list_entries[ chunk_index ] )
		{
			this->m_cached_list_entries[ chunk_index ] = memory::read<std::uintptr_t>(
				entity_list + ( static_cast< std::uintptr_t >( chunk_index ) * 8 ) + 0x10 );
		}

		const auto list_entry = this->m_cached_list_entries[ chunk_index ];
		if ( !memory::is_game_ptr( list_entry ) )
		{
			this->m_cached_list_entries[ chunk_index ] = 0;
			return 0;
		}

		const auto identity = list_entry + ( static_cast< std::uintptr_t >( index & 0x1ff ) * 112 );
		if ( memory::read<std::uint32_t>( identity + 0x10 ) != handle )
		{
			return 0;
		}

		const auto entity = memory::read<std::uintptr_t>( identity );
		if ( !memory::is_game_ptr( entity ) )
		{
			return 0;
		}

		return entity;
	}

	std::uint32_t entities::player_pawn_handle( std::uintptr_t controller ) const
	{
		if ( !memory::is_game_ptr( controller ) )
		{
			return 0;
		}

		const auto handle = reinterpret_cast<CCSPlayerController*>( controller )->m_hPlayerPawn( );
		if ( !handle || handle == 0xffffffffu || handle == 0xfffffffeu )
		{
			return 0;
		}

		return handle;
	}

	std::uintptr_t entities::player_pawn( std::uintptr_t controller ) const
	{
		const auto handle = this->player_pawn_handle( controller );
		if ( !handle )
		{
			return 0;
		}

		const auto pawn = this->lookup( handle );
		if ( !memory::is_game_ptr( pawn ) || !this->is_cs_player_pawn( pawn ) )
		{
			return 0;
		}

		return pawn;
	}

	std::uintptr_t entities::observer_pawn( std::uintptr_t controller ) const
	{
		if ( !memory::is_game_ptr( controller ) )
		{
			return 0;
		}

		const auto handle = reinterpret_cast<CCSPlayerController*>( controller )->m_hObserverPawn( );
		const auto pawn = this->lookup( handle );
		if ( !this->is_player_pawn_base( pawn ) )
		{
			return 0;
		}

		return pawn;
	}

	std::uintptr_t entities::observer_services( std::uintptr_t pawn ) const
	{
		if ( !this->is_player_pawn_base( pawn ) )
		{
			return 0;
		}

		const auto off = SCHEMA_OFFSET( "C_BasePlayerPawn", "m_pObserverServices"_hash );
		if ( !off )
		{
			return 0;
		}

		const auto services = memory::read<std::uintptr_t>( pawn + off );
		if ( !memory::is_game_ptr( services ) )
		{
			return 0;
		}

		return services;
	}

	std::uintptr_t entities::observer_target( std::uintptr_t pawn ) const
	{
		const auto services = this->observer_services( pawn );
		if ( !services )
		{
			return 0;
		}

		const auto mode_off = SCHEMA_OFFSET( "CPlayer_ObserverServices", "m_iObserverMode"_hash );
		const auto target_off = SCHEMA_OFFSET( "CPlayer_ObserverServices", "m_hObserverTarget"_hash );
		if ( !mode_off || !target_off )
		{
			return 0;
		}

		if ( !memory::read<std::uint8_t>( services + mode_off ) )
		{
			return 0;
		}

		const auto target = this->lookup( memory::read<std::uint32_t>( services + target_off ) );
		if ( !this->is_player_pawn_base( target ) )
		{
			return 0;
		}

		return target;
	}

	bool entities::is_cs_player_pawn( std::uintptr_t entity ) const
	{
		const auto name = this->get_schema_name( entity );
		return name && fnv1a::runtime_hash( name ) == "C_CSPlayerPawn"_hash;
	}

	bool entities::is_player_pawn_base( std::uintptr_t entity ) const
	{
		const auto name = this->get_schema_name( entity );
		if ( !name )
		{
			return false;
		}

		const auto hashed = fnv1a::runtime_hash( name );
		return hashed == "C_CSPlayerPawn"_hash
			|| hashed == "C_CSObserverPawn"_hash
			|| hashed == "C_CSPlayerPawnBase"_hash;
	}

	std::vector<entities::cached> entities::get_by_type( type type ) const
	{
		std::shared_lock lock( this->m_cache_mtx );

		std::vector<cached> result{};
		result.reserve( this->m_cached.size( ) );

		for ( const auto& c : this->m_cached )
		{
			if ( c.type == type )
			{
				result.emplace_back( c );
			}
		}

		return result;
	}

	bool entities::is_empty( ) const
	{
		std::shared_lock lock( this->m_cache_mtx );
		return this->m_cached.empty( );
	}

	entities::type entities::classify_entity( std::uint32_t schema_hash ) const
	{
		switch ( schema_hash )
		{
		case "CCSPlayerController"_hash:
			return type::player;

		case "C_AK47"_hash:
		case "C_WeaponM4A1"_hash:
		case "C_WeaponM4A1Silencer"_hash:
		case "C_WeaponAWP"_hash:
		case "C_WeaponAug"_hash:
		case "C_WeaponFamas"_hash:
		case "C_WeaponGalilAR"_hash:
		case "C_WeaponSG556"_hash:
		case "C_WeaponG3SG1"_hash:
		case "C_WeaponSCAR20"_hash:
		case "C_WeaponSSG08"_hash:
		case "C_WeaponMAC10"_hash:
		case "C_WeaponMP5SD"_hash:
		case "C_WeaponMP7"_hash:
		case "C_WeaponMP9"_hash:
		case "C_WeaponBizon"_hash:
		case "C_WeaponP90"_hash:
		case "C_WeaponUMP45"_hash:
		case "C_WeaponNOVA"_hash:
		case "C_WeaponSawedoff"_hash:
		case "C_WeaponXM1014"_hash:
		case "C_WeaponMag7"_hash:
		case "C_WeaponM249"_hash:
		case "C_WeaponNegev"_hash:
		case "C_DEagle"_hash:
		case "C_WeaponElite"_hash:
		case "C_WeaponFiveSeven"_hash:
		case "C_WeaponGlock"_hash:
		case "C_WeaponHKP2000"_hash:
		case "C_WeaponUSPSilencer"_hash:
		case "C_WeaponP250"_hash:
		case "C_WeaponCZ75a"_hash:
		case "C_WeaponTec9"_hash:
		case "C_WeaponRevolver"_hash:
		case "C_WeaponTaser"_hash:
		case "C_Knife"_hash:
		case "C_C4"_hash:
		case "C_Item_Healthshot"_hash:
		case "C_HEGrenade"_hash:
		case "C_Flashbang"_hash:
		case "C_SmokeGrenade"_hash:
		case "C_MolotovGrenade"_hash:
		case "C_IncendiaryGrenade"_hash:
		case "C_DecoyGrenade"_hash:
			return type::item;

		case "C_HEGrenadeProjectile"_hash:
		case "C_FlashbangProjectile"_hash:
		case "C_SmokeGrenadeProjectile"_hash:
		case "C_MolotovProjectile"_hash:
		case "C_Inferno"_hash:
		case "C_DecoyProjectile"_hash:
			return type::projectile;

		case "C_Chicken"_hash:
			return type::chicken;

		default:
			return type::unknown;
		}
	}

}




