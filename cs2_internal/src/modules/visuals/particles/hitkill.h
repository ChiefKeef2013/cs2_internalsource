#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/settings.hpp>
#include <core/common.hpp>
#include <modules/visuals/particles/manager.h>
#include <modules/visuals/particles/vpcf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <vector>

namespace features::world {

	class hitkill_particles
	{
	public:
		enum class special_kind : std::uint8_t
		{
			none,
			stars,
			fade
		};

		struct preset
		{
			const char* label;
			const char* effect;
			special_kind special;
			unsigned long long lifetime_ms;
		};

		static constexpr preset hit_presets[] = {
			{ "incendiary air", vpcf::hit_kill::incendiary_air, special_kind::none, 3600 },
			{ "kill stars", vpcf::hit_kill::kill_stars, special_kind::stars, 2600 },
			{ "fade", vpcf::hit_kill::fade, special_kind::fade, 2400 },
			{ "ash burning", vpcf::hit_kill::ash_burning, special_kind::none, 2800 }
		};

		static constexpr preset kill_presets[] = {
			{ "incendiary air", vpcf::hit_kill::incendiary_air, special_kind::none, 3600 },
			{ "mail debris", vpcf::hit_kill::mail_debris, special_kind::none, 2600 },
			{ "c4 explosion", vpcf::hit_kill::c4_explosion, special_kind::none, 4600 },
			{ "kill stars", vpcf::hit_kill::kill_stars, special_kind::stars, 2600 },
			{ "fade", vpcf::hit_kill::fade, special_kind::fade, 2400 },
			{ "ghost whisps", vpcf::hit_kill::ghost_whisps, special_kind::none, 3200 },
			{ "chicken feathers", vpcf::hit_kill::chicken_feathers, special_kind::none, 2800 },
			{ "ash burning", vpcf::hit_kill::ash_burning, special_kind::none, 3000 },
			{ "he snow", vpcf::hit_kill::he_snow, special_kind::none, 3600 },
			{ "lightning blue", vpcf::hit_kill::lightning_blue, special_kind::none, 2800 },
			{ "lightning green", vpcf::hit_kill::lightning_green, special_kind::none, 2800 },
			{ "lightning purple", vpcf::hit_kill::lightning_purple, special_kind::none, 2800 },
			{ "explosion blue", vpcf::hit_kill::explosion_blue, special_kind::none, 4200 },
			{ "explosion green", vpcf::hit_kill::explosion_green, special_kind::none, 4200 },
			{ "explosion purple", vpcf::hit_kill::explosion_purple, special_kind::none, 4200 },
			{ "explosion color", vpcf::hit_kill::explosion_color, special_kind::none, 4200 },
			{ "poxian explosion", vpcf::hit_kill::poxian_exp, special_kind::none, 3600 }
		};

		static constexpr int hit_preset_count = static_cast< int >( std::size( hit_presets ) );
		static constexpr int kill_preset_count = static_cast< int >( std::size( kill_presets ) );
		static constexpr const char* hit_preset_labels[] = {
			"incendiary air", "kill stars", "fade", "ash burning"
		};
		static constexpr const char* kill_preset_labels[] = {
			"incendiary air", "mail debris", "c4 explosion", "kill stars", "fade",
			"ghost whisps", "chicken feathers", "ash burning", "he snow",
			"lightning blue", "lightning green", "lightning purple",
			"explosion blue", "explosion green", "explosion purple", "explosion color", "poxian explosion"
		};

		void on_player_hurt( std::uintptr_t victim_pawn, bool is_kill );
		void on_frame_stage_notify( );
		void reset( );

	private:
		struct timed_effect
		{
			std::int32_t effect_id;
			unsigned long long expire_time;
		};

		static constexpr std::size_t max_active = 32;

		[[nodiscard]] math::vector3 read_special_color( special_kind kind ) const;
		[[nodiscard]] bool spawn_bound( const preset& preset, std::uintptr_t pawn, unsigned long long lifetime_ms, std::int32_t* effect_id );
		[[nodiscard]] bool spawn_colored_world( const preset& preset, const math::vector3& origin, unsigned long long lifetime_ms, std::int32_t* effect_id );
		[[nodiscard]] bool spawn_preset( const preset& preset, std::uintptr_t pawn, const math::vector3& origin, unsigned long long lifetime_ms, std::int32_t* effect_id );
		[[nodiscard]] bool pawn_hit_origin( std::uintptr_t pawn, math::vector3& origin ) const;
		void track( std::int32_t effect_id, unsigned long long lifetime_ms );
		void prune( );
		[[nodiscard]] int current_hit_preset( ) const;
		[[nodiscard]] int current_kill_preset( ) const;

		std::vector<timed_effect> m_active{};
	};

	inline math::vector3 hitkill_particles::read_special_color( special_kind kind ) const
	{
		const auto& cfg = settings::g_world.m_particles;
		const xdraw::color* color{};
		switch ( kind )
		{
		case special_kind::stars: color = &cfg.hitkill_stars_color.value; break;
		case special_kind::fade: color = &cfg.hitkill_fade_color.value; break;
		default: break;
		}

		if ( !color )
			return { 255.0f, 255.0f, 255.0f };

		return {
			static_cast< float >( color->r ),
			static_cast< float >( color->g ),
			static_cast< float >( color->b )
		};
	}

	inline bool hitkill_particles::spawn_bound( const preset& preset, std::uintptr_t pawn, unsigned long long lifetime_ms, std::int32_t* effect_id )
	{
		if ( !pawn || preset.special != special_kind::fade )
			return false;

		std::uint32_t particle_index = 0;
		if ( !particles::create( preset.effect, particle_index ) || particle_index == 0 )
			return false;

		const auto color = this->read_special_color( preset.special );
		particles::set_control_point( particle_index, 2, color );
		const auto first = particles::set_entity_binding( particle_index, 0, reinterpret_cast< void* >( pawn ) );
		const auto second = particles::set_entity_binding( particle_index, 1, reinterpret_cast< void* >( pawn ) );
		if ( !first || !second )
		{
			particles::destroy_particle( particle_index );
			return false;
		}

		particles::track_effect( static_cast< std::int32_t >( particle_index ), lifetime_ms );
		if ( effect_id )
			*effect_id = static_cast< std::int32_t >( particle_index );

		return true;
	}

	inline bool hitkill_particles::spawn_colored_world( const preset& preset, const math::vector3& origin, unsigned long long lifetime_ms, std::int32_t* effect_id )
	{
		if ( !std::isfinite( origin.x ) || preset.special != special_kind::stars )
		{
			return false;
		}

		std::uint32_t particle_index = 0;
		if ( !particles::create( preset.effect, particle_index ) || particle_index == 0 )
			return false;

		const auto color = this->read_special_color( preset.special );
		if ( !particles::set_control_point( particle_index, 0, origin ) )
		{
			particles::destroy_particle( particle_index );
			return false;
		}

		particles::set_control_point( particle_index, 1, color );
		particles::track_effect( static_cast< std::int32_t >( particle_index ), lifetime_ms );
		if ( effect_id )
			*effect_id = static_cast< std::int32_t >( particle_index );

		return true;
	}

	inline bool hitkill_particles::spawn_preset( const preset& preset, std::uintptr_t pawn, const math::vector3& origin, unsigned long long lifetime_ms, std::int32_t* effect_id )
	{
		switch ( preset.special )
		{
		case special_kind::fade:
			return this->spawn_bound( preset, pawn, lifetime_ms, effect_id );
		case special_kind::stars:
			return this->spawn_colored_world( preset, origin, lifetime_ms, effect_id );
		default:
			return particles::spawn_one_shot( preset.effect, origin, lifetime_ms, effect_id );
		}
	}

	inline bool hitkill_particles::pawn_hit_origin( std::uintptr_t pawn, math::vector3& origin ) const
	{
		if ( !pawn )
			return false;

		const auto scene_node = reinterpret_cast< C_BaseEntity* >( pawn )->m_pGameSceneNode( );
		if ( !scene_node )
			return false;

		origin = reinterpret_cast< CGameSceneNode* >( scene_node )->m_vecAbsOrigin( );
		origin.z += 46.0f;
		return std::isfinite( origin.x ) && std::isfinite( origin.y ) && std::isfinite( origin.z );
	}

	inline void hitkill_particles::track( std::int32_t effect_id, unsigned long long lifetime_ms )
	{
		this->m_active.push_back( { effect_id, GetTickCount64( ) + lifetime_ms } );
		if ( this->m_active.size( ) > max_active )
			this->m_active.erase( this->m_active.begin( ), this->m_active.begin( ) + ( this->m_active.size( ) - max_active ) );
	}

	inline void hitkill_particles::prune( )
	{
		const auto now = GetTickCount64( );
		for ( auto it = this->m_active.begin( ); it != this->m_active.end( ); )
		{
			if ( now >= it->expire_time )
				it = this->m_active.erase( it );
			else
				++it;
		}
	}

	inline int hitkill_particles::current_hit_preset( ) const
	{
		const auto preset = settings::g_world.m_particles.hit_preset.value;
		return preset >= 0 && preset < hit_preset_count ? preset : 0;
	}

	inline int hitkill_particles::current_kill_preset( ) const
	{
		const auto preset = settings::g_world.m_particles.kill_preset.value;
		return preset >= 0 && preset < kill_preset_count ? preset : 0;
	}

	inline void hitkill_particles::on_player_hurt( std::uintptr_t victim_pawn, bool is_kill )
	{
		const auto& cfg = settings::g_world.m_particles;
		if ( ( !cfg.hit.value && !cfg.kill.value ) || !victim_pawn || !particles::effects_ready( ) )
			return;

		this->prune( );

		math::vector3 origin{};
		if ( !this->pawn_hit_origin( victim_pawn, origin ) )
			return;

		if ( cfg.hit.value )
		{
			const auto& effect = hit_presets[ this->current_hit_preset( ) ];
			std::int32_t effect_id = -1;
			if ( this->spawn_preset( effect, victim_pawn, origin, effect.lifetime_ms, &effect_id ) )
				this->track( effect_id, effect.lifetime_ms );
		}

		if ( is_kill && cfg.kill.value )
		{
			const auto& effect = kill_presets[ this->current_kill_preset( ) ];
			std::int32_t effect_id = -1;
			if ( this->spawn_preset( effect, victim_pawn, origin, effect.lifetime_ms, &effect_id ) )
				this->track( effect_id, effect.lifetime_ms );
		}
	}

	inline void hitkill_particles::on_frame_stage_notify( )
	{
		const auto& cfg = settings::g_world.m_particles;
		if ( cfg.hit.value )
			particles::prewarm( hit_presets[ this->current_hit_preset( ) ].effect );
		if ( cfg.kill.value )
			particles::prewarm( kill_presets[ this->current_kill_preset( ) ].effect );

		this->prune( );
	}

	inline void hitkill_particles::reset( )
	{
		this->m_active.clear( );
	}

	inline hitkill_particles g_hitkill_particles{};

} // namespace features::world


