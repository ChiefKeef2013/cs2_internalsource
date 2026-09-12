#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>

#include <imgui.h>

#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerController.h>
#include <valve/schemas/CBasePlayerPawn.h>

#include <Windows.h>
#include <vector>

namespace features::misc {

	namespace enemy_spec_detail {

		constexpr std::uint8_t k_obs_in_eye = 2;
		constexpr std::uint8_t k_obs_chase = 3;

		inline bool any_down( const int* keys, int n )
		{
			for ( int i = 0; i < n; ++i )
			{
				if ( ( GetAsyncKeyState( keys[ i ] ) & 0x8000 ) != 0 )
				{
					return true;
				}
			}
			return false;
		}

		inline bool group_edge( const int* keys, int n, bool& latch )
		{
			const bool down = any_down( keys, n );
			if ( down && !latch )
			{
				latch = true;
				return true;
			}
			if ( !down )
			{
				latch = false;
			}
			return false;
		}

		struct slot
		{
			std::uint32_t handle{};
			int ent_index{};
			std::uintptr_t pawn{};
		};

		[[nodiscard]] inline bool pawn_readable( std::uintptr_t pawn )
		{
			if ( !memory::is_game_ptr( pawn ) )
			{
				return false;
			}

			const auto identity = memory::safe_read<std::uintptr_t>( pawn + 0x10 );
			return identity && memory::is_game_ptr( *identity );
		}

		[[nodiscard]] inline std::uintptr_t observer_services( std::uintptr_t pawn )
		{
			return systems::g_entities.observer_services( pawn );
		}

		inline void write_target( std::uintptr_t obs_services, std::uint32_t handle, std::uint8_t mode )
		{
			if ( !memory::is_game_ptr( obs_services ) || !handle || handle == 0xffffffffu || handle == 0xfffffffeu )
			{
				return;
			}

			const auto mode_off = SCHEMA_OFFSET( "CPlayer_ObserverServices", "m_iObserverMode"_hash );
			const auto target_off = SCHEMA_OFFSET( "CPlayer_ObserverServices", "m_hObserverTarget"_hash );
			if ( !mode_off || !target_off )
			{
				return;
			}

			if ( !memory::safe_write< std::uint8_t >( obs_services + mode_off, mode ) )
			{
				return;
			}

			( void )memory::safe_write< std::uint32_t >( obs_services + target_off, handle );
		}

		inline void apply_to_pawn( std::uintptr_t pawn, std::uint32_t handle, std::uint8_t mode )
		{
			write_target( observer_services( pawn ), handle, mode );
		}

	}

	void enemy_spec::on_frame( )
	{
		using namespace enemy_spec_detail;

		if ( systems::g_lifecycle.busy( ) || !settings::g_misc.m_spectate.enabled.value )
		{
			this->m_was_dead = false;
			this->m_target_idx = -1;
			this->m_slot = 0;
			return;
		}

		if ( ImGui::GetIO( ).WantCaptureMouse )
		{
			return;
		}

		const auto local = systems::g_local.get( );
		if ( !pawn_readable( local.pawn ) )
		{
			return;
		}

		const auto hp = reinterpret_cast< C_BaseEntity* >( local.pawn )->m_iHealth( );
		const auto life = reinterpret_cast< C_BaseEntity* >( local.pawn )->m_lifeState( );
		const bool alive = ( hp > 0 && life == 0 );
		if ( alive )
		{
			this->m_was_dead = false;
			this->m_target_idx = -1;
			this->m_slot = 0;
			return;
		}

		std::uintptr_t spec_pawn = systems::g_entities.observer_pawn( local.controller );
		if ( !observer_services( spec_pawn ) )
		{
			spec_pawn = local.pawn;
			if ( !observer_services( spec_pawn ) )
			{
				return;
			}
		}

		std::vector<slot> alive_players{};
		alive_players.reserve( 16 );

		const auto players = systems::g_entities.get_by_type( systems::entities::type::player );
		for ( const auto& e : players )
		{
			if ( !memory::is_game_ptr( e.ptr ) || e.ptr == local.controller )
			{
				continue;
			}

			const auto is_alive = reinterpret_cast< CCSPlayerController* >( e.ptr )->m_bPawnIsAlive( );
			if ( !is_alive )
			{
				continue;
			}

			const auto hpawn = systems::g_entities.player_pawn_handle( e.ptr );
			if ( !hpawn )
			{
				continue;
			}

			const auto pawn = systems::g_entities.lookup( hpawn );
			if ( !pawn_readable( pawn ) || pawn == local.pawn )
			{
				continue;
			}

			const auto p_hp = reinterpret_cast< C_BaseEntity* >( pawn )->m_iHealth( );
			if ( p_hp <= 0 )
			{
				continue;
			}

			slot s{};
			s.handle = hpawn;
			s.ent_index = e.index;
			s.pawn = pawn;
			alive_players.emplace_back( s );
		}

		if ( alive_players.empty( ) )
		{
			return;
		}

		std::sort( alive_players.begin( ), alive_players.end( ),
			[]( const slot& a, const slot& b ) { return a.ent_index < b.ent_index; } );

		if ( !this->m_was_dead )
		{
			this->m_slot = 0;
			this->m_target_idx = alive_players[ 0 ].ent_index;
			this->m_was_dead = true;
			this->m_edge_next = false;
			this->m_edge_prev = false;
		}

		static const int k_next_keys[] = { VK_LBUTTON, VK_RIGHT, VK_OEM_6, VK_XBUTTON2, VK_NEXT };
		static const int k_prev_keys[] = { VK_RBUTTON, VK_LEFT, VK_OEM_4, VK_XBUTTON1, VK_PRIOR };

		const int n = static_cast< int >( alive_players.size( ) );
		if ( group_edge( k_next_keys, 5, this->m_edge_next ) )
		{
			this->m_slot = ( this->m_slot + 1 ) % n;
			this->m_target_idx = alive_players[ this->m_slot ].ent_index;
		}
		if ( group_edge( k_prev_keys, 5, this->m_edge_prev ) )
		{
			this->m_slot = ( this->m_slot - 1 + n ) % n;
			this->m_target_idx = alive_players[ this->m_slot ].ent_index;
		}

		int pick = -1;
		for ( int i = 0; i < n; ++i )
		{
			if ( alive_players[ i ].ent_index == this->m_target_idx )
			{
				pick = i;
				break;
			}
		}
		if ( pick < 0 )
		{
			this->m_slot = std::clamp( this->m_slot, 0, n - 1 );
			pick = this->m_slot;
			this->m_target_idx = alive_players[ pick ].ent_index;
		}
		else
		{
			this->m_slot = pick;
		}

		const std::uint32_t target_handle = alive_players[ pick ].handle;
		if ( !target_handle || target_handle == 0xffffffffu || target_handle == 0xfffffffeu )
		{
			return;
		}

		const std::uint8_t mode = settings::g_misc.m_spectate.thirdperson.value ? k_obs_chase : k_obs_in_eye;

		apply_to_pawn( spec_pawn, target_handle, mode );
		if ( local.pawn != spec_pawn && observer_services( local.pawn ) )
		{
			apply_to_pawn( local.pawn, target_handle, mode );
		}
	}

}
