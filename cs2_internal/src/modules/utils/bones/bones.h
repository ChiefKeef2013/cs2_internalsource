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

	bones::data bones::get( std::uintptr_t entity, std::uint32_t bone_id )
	{
		data result{};

		auto count{ 0 };
		const auto cache = this->get_bone_cache( entity, &count );

		if ( !cache || bone_id >= count )
		{
			return result;
		}

		const auto& bone = *reinterpret_cast< data* >( cache + static_cast< std::size_t >( bone_id ) * sizeof( data ) );
		result.position = bone.position;
		result.rotation = bone.rotation;

		return result;
	}

	std::array<bones::data, 27> bones::get_skeleton( std::uintptr_t entity )
	{
		static constexpr std::array<std::uint32_t, 21> k_bones
		{
			cstypes::bone_ids::pelvis,
			cstypes::bone_ids::spine_1,
			cstypes::bone_ids::spine_2,
			cstypes::bone_ids::spine_3,
			cstypes::bone_ids::spine_4,
			cstypes::bone_ids::neck,
			cstypes::bone_ids::head,
			cstypes::bone_ids::left_clavicle,
			cstypes::bone_ids::left_shoulder,
			cstypes::bone_ids::left_elbow,
			cstypes::bone_ids::left_hand,
			cstypes::bone_ids::right_clavicle,
			cstypes::bone_ids::right_shoulder,
			cstypes::bone_ids::right_elbow,
			cstypes::bone_ids::right_hand,
			cstypes::bone_ids::left_hip,
			cstypes::bone_ids::left_knee,
			cstypes::bone_ids::left_foot,
			cstypes::bone_ids::right_hip,
			cstypes::bone_ids::right_knee,
			cstypes::bone_ids::right_foot
		};

		std::array<data, 27> skeleton{};

		auto count{ 0 };
		const auto cache = this->get_bone_cache( entity, &count );

		if ( !cache || count <= 0 )
		{
			return skeleton;
		}

		for ( const auto bone_id : k_bones )
		{
			if ( bone_id >= count )
			{
				continue;
			}

			const auto& bone = *reinterpret_cast< data* >( cache + static_cast< std::size_t >( bone_id ) * sizeof( data ) );
			skeleton[ bone_id ].position = bone.position;
			skeleton[ bone_id ].rotation = bone.rotation;
		}

		return skeleton;
	}

	std::uintptr_t bones::get_bone_cache( std::uintptr_t entity, int* out_count )
	{
		if ( !memory::is_game_ptr( entity ) )
		{
			return 0;
		}

		const auto vtable = memory::read<std::uintptr_t>( entity );
		if ( !memory::is_game_ptr( vtable ) )
			return 0;

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( entity )->m_pGameSceneNode();
		if ( !memory::is_game_ptr( game_scene_node ) )
		{
			return 0;
		}

		const auto bone_cache = memory::read<std::uintptr_t>( game_scene_node + SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash ) + 0x80 );
		if ( !memory::is_game_ptr( bone_cache ) )
		{
			return 0;
		}

		if ( out_count )
		{
			const auto count = memory::read<int>( game_scene_node + SCHEMA_OFFSET( "CSkeletonInstance", "m_modelState"_hash ) + 0x8c );
			if ( count <= 0 || count > 128 )
			{
				*out_count = 0;
				return 0;
			}

			*out_count = count;
		}

		return bone_cache;
	}

}




