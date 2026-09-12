#pragma once

#include <core/memory.hpp>
#include <core/menu/rendering.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <unordered_set>
#include <imgui.h>
#include <core/menu/Framework/Framework/Src/framework/headers/fonts.h>
#include <core/menu/Framework/Framework/Src/framework/data/fonts.h>

namespace features::esp::item {

	namespace detail {
		inline ImFont* get_esp_font( )
		{
			if ( !font )
			{
				return nullptr;
			}
			auto* f = font->get( smallest_pixel_font_data, rendering::g_fonts.esp_name_size );
		if ( !f || !f->IsLoaded( ) || f == ImGui::GetDefaultFont( ) )
			{
				return nullptr;
			}
			rendering::g_fonts.esp_name = f;
			return f;
		}

		inline ImVec2 measure_esp_text( const std::string& text )
		{
			auto* f = get_esp_font( );
			if ( !f )
			{
				return ImVec2( 0.0f, 0.0f );
			}
			return f->CalcTextSizeA( f->FontSize, FLT_MAX, 0.0f, text.c_str( ) );
		}

		inline void draw_esp_text_outlined( float x, float y, const std::string& text, xdraw::color col )
		{
			auto* f = get_esp_font( );
			auto* bg = ImGui::GetBackgroundDrawList( );
			if ( !f || !bg || text.empty( ) || col.a == 0 )
			{
				return;
			}

			const ImU32 fg = IM_COL32( col.r, col.g, col.b, col.a );
			const ImU32 shadow = IM_COL32( 0, 0, 0, col.a );

			constexpr float offsets[ 8 ][ 2 ] = {
				{ -1.f, -1.f }, { -1.f, 1.f }, { 1.f, -1.f }, { 1.f, 1.f },
				{  0.f,  1.f }, {  1.f, 0.f }, { 0.f, -1.f }, { -1.f, 0.f }
			};
			for ( const auto& [ox, oy] : offsets )
			{
				bg->AddText( f, f->FontSize, ImVec2( x + ox, y + oy ), shadow, text.c_str( ) );
			}
			bg->AddText( f, f->FontSize, ImVec2( x, y ), fg, text.c_str( ) );
		}
	}

	void overlay::on_render( xdraw::draw_list& draw_list )
	{
		const auto& overlay_cfg = settings::g_esp.m_item.m_overlay;
		if ( !overlay_cfg.enabled.value )
		{
			this->m_fade_alpha.clear( );
			return;
		}

		std::unordered_set<std::uintptr_t> seen_this_frame;
		seen_this_frame.reserve( this->m_fade_alpha.size( ) + 16 );

		const auto now = std::chrono::steady_clock::now( );
		const auto delta_s = this->m_last_tick.time_since_epoch( ).count( ) > 0
			? std::chrono::duration<float>( now - this->m_last_tick ).count( )
			: 0.0f;
		this->m_last_tick = now;
		const auto fade_speed = 6.5f;

		for ( const auto& item : systems::g_entities.get_by_type( systems::entities::type::item ) )
		{
			const auto info = this->get_info( item );
			if ( !info.valid( ) )
			{
				continue;
			}

			const auto& grp = overlay_cfg.get_group( 0 );

			const auto max_dist = grp.max_distance.value;
			float target_alpha = 1.0f;
			if ( max_dist > 0.0f )
			{
				const auto fade_start = max_dist * 0.85f;
				if ( info.distance >= max_dist )
					target_alpha = 0.0f;
				else if ( info.distance > fade_start )
					target_alpha = 1.0f - ( info.distance - fade_start ) / ( max_dist - fade_start );
			}

			auto& state = this->m_fade_alpha[ item.ptr ];
			state = state + ( target_alpha - state ) * std::min( 1.0f, delta_s * fade_speed );
			seen_this_frame.insert( item.ptr );

			if ( state <= 0.01f )
				continue;

			this->add_label( draw_list, info, grp, state );
		}

		for ( auto it = this->m_fade_alpha.begin( ); it != this->m_fade_alpha.end( ); )
		{
			if ( !seen_this_frame.contains( it->first ) )
			{
				it->second -= delta_s * fade_speed;
				if ( it->second <= 0.01f )
				{
					it = this->m_fade_alpha.erase( it );
					continue;
				}
			}
			++it;
		}
	}

	void overlay::add_label( xdraw::draw_list& draw_list, const info& info, const settings::esp::item::overlay::group& cfg, float fade_alpha )
	{
		const auto screen = systems::g_view.project( info.origin );
		if ( !systems::g_view.projection_valid( screen ) )
		{
			return;
		}

		const auto scale_a = [ fade_alpha ]( xdraw::color c )
		{
			c.a = static_cast<std::uint8_t>( std::clamp( static_cast<float>( c.a ) * fade_alpha, 0.0f, 255.0f ) );
			return c;
		};

		const auto show_icon = cfg.display == settings::esp::item::overlay::group::display_type::icon || cfg.display == settings::esp::item::overlay::group::display_type::text_and_icon;
		const auto show_text = cfg.display == settings::esp::item::overlay::group::display_type::text || cfg.display == settings::esp::item::overlay::group::display_type::text_and_icon;
		auto y = screen.y;

		if ( show_icon )
		{
			const auto ico = systems::g_icons.get( info.schema_hash, 0.35f );
			if ( ico && ico->texture )
			{
				const auto iw = static_cast< float >( ico->width );
				const auto ih = static_cast< float >( ico->height );
				const auto ix = std::floorf( screen.x - iw * 0.5f );
				const auto iy = std::floorf( y );

				const auto outline = scale_a( xdraw::color{ 0, 0, 0, 255 } );

				draw_list.image( ix - 1.0f, iy, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix + 1.0f, iy, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy - 1.0f, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy + 1.0f, iw, ih, ico->texture.Get( ), outline );
				draw_list.image( ix, iy, iw, ih, ico->texture.Get( ), scale_a( cfg.icon_color ) );

				y += ih + 1.0f;
			}
		}

		if ( show_text )
		{
			const auto sz = detail::measure_esp_text( info.name );
			detail::draw_esp_text_outlined( std::floorf( screen.x - sz.x * 0.5f ), std::floorf( y ), info.name, scale_a( cfg.text_color ) );
			( void )draw_list;
		}
	}

	overlay::info overlay::get_info( const systems::entities::cached& entity )
	{
		info info{};
		info.entity = entity.ptr;
		info.schema_hash = entity.schema_hash;

		if ( !info.entity )
		{
			return info;
		}

		const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( info.entity )->m_pGameSceneNode( );
		if ( !game_scene_node )
		{
			return info;
		}

		if ( reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_bDormant( ) )
		{
			return info;
		}

		const auto owner_handle = reinterpret_cast<C_BaseEntity*>( info.entity )->m_hOwnerEntity( );
		if ( owner_handle && owner_handle != 0xffffffff )
		{
			return info;
		}

		info.origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin( );

		math::vector3 dist_origin = systems::g_view.origin( );
		if ( const auto local = systems::g_local.get( ); local.is_valid( ) )
		{
			if ( const auto local_node = reinterpret_cast<C_BaseEntity*>( local.pawn )->m_pGameSceneNode( ) )
				dist_origin = reinterpret_cast<CGameSceneNode*>( local_node )->m_vecAbsOrigin( );
		}
		info.distance = dist_origin.distance( info.origin );
		info.vdata = memory::read<std::uintptr_t>( info.entity + SCHEMA_OFFSET( "C_BaseEntity", "m_nSubclassID"_hash ) + 0x8 );

		if ( !info.vdata )
		{
			return info;
		}

		const auto name_ptr = reinterpret_cast<CCSWeaponBaseVData*>( info.vdata )->m_szName( );
		if ( !name_ptr )
		{
			return info;
		}

		info.name = memory::read_string( name_ptr, 64 );

		if ( info.name.starts_with( "weapon_" ) )
		{
			info.name.erase( 0, 7 );
		}

		return info;
	}

	std::uint32_t overlay::get_item_group( std::uint32_t schema_hash )
	{
		switch ( schema_hash )
		{
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
			return 0;

		case "C_WeaponMAC10"_hash:
		case "C_WeaponMP5SD"_hash:
		case "C_WeaponMP7"_hash:
		case "C_WeaponMP9"_hash:
		case "C_WeaponBizon"_hash:
		case "C_WeaponP90"_hash:
		case "C_WeaponUMP45"_hash:
			return 1;

		case "C_AK47"_hash:
		case "C_WeaponM4A1"_hash:
		case "C_WeaponM4A1Silencer"_hash:
		case "C_WeaponAug"_hash:
		case "C_WeaponFamas"_hash:
		case "C_WeaponGalilAR"_hash:
		case "C_WeaponSG556"_hash:
			return 2;

		case "C_WeaponNOVA"_hash:
		case "C_WeaponSawedoff"_hash:
		case "C_WeaponXM1014"_hash:
		case "C_WeaponMag7"_hash:
			return 3;

		case "C_WeaponAWP"_hash:
		case "C_WeaponG3SG1"_hash:
		case "C_WeaponSCAR20"_hash:
		case "C_WeaponSSG08"_hash:
		case "C_WeaponM249"_hash:
		case "C_WeaponNegev"_hash:
			return 4;

		case "C_HEGrenade"_hash:
		case "C_Flashbang"_hash:
		case "C_SmokeGrenade"_hash:
		case "C_MolotovGrenade"_hash:
		case "C_IncendiaryGrenade"_hash:
		case "C_DecoyGrenade"_hash:
		case "C_C4"_hash:
		case "C_WeaponTaser"_hash:
		case "C_Item_Healthshot"_hash:
		case "C_Knife"_hash:
			return 5;

		default:
			return UINT32_MAX;
		}
	}

}
