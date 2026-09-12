#pragma once

#include <core/memory.hpp>
#include <core/common.hpp>
#include <core/menu/rendering.hpp>
#include <core/settings.hpp>
#include <core/features.hpp>
#include <unordered_set>
#include <chrono>
#include <imgui.h>
#include <core/menu/Framework/Framework/Src/framework/headers/fonts.h>
#include <core/menu/Framework/Framework/Src/framework/data/fonts.h>
#include <core/menu/Framework/Framework/Src/framework/headers/functions.h>

namespace features::esp::other {

	namespace detail {

		inline ImFont* get_esp_font( )
		{
			if ( !font )
			{
				return nullptr;
			}

			auto* f = font->get( smallest_pixel_font_data, rendering::g_fonts.esp_name_size );
			if ( !f || !f->IsLoaded( ) )
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

		inline void draw_esp_text( xdraw::draw_list& draw_list, float x, float y, const std::string& text, xdraw::color col, bool outlined )
		{
			auto* f = get_esp_font( );
			ImDrawList* dl = draw_list.imgui ? draw_list.imgui : ImGui::GetBackgroundDrawList( );
			if ( !f || !dl || text.empty( ) || col.a == 0 )
			{
				return;
			}

			const ImU32 fg = IM_COL32( col.r, col.g, col.b, col.a );
			const ImU32 shadow = IM_COL32( 0, 0, 0, col.a );
			const float size = f->FontSize;
			( void )outlined;

			constexpr float offsets[ 8 ][ 2 ] = {
				{ -1.f, -1.f }, { -1.f, 1.f }, { 1.f, -1.f }, { 1.f, 1.f },
				{  0.f,  1.f }, {  1.f, 0.f }, { 0.f, -1.f }, { -1.f, 0.f }
			};
			for ( const auto& [ ox, oy ] : offsets )
			{
				dl->AddText( f, size, ImVec2( x + ox, y + oy ), shadow, text.c_str( ) );
			}

			dl->AddText( f, size, ImVec2( x, y ), fg, text.c_str( ) );
		}

		inline void draw_overlay_chrome( xdraw::draw_list& draw_list, float x, float y, float w, float h, const xdraw::color& accent, std::uint8_t alpha = 255 )
		{
			constexpr auto rounding{ 12.0f };
			const auto body = xdraw::color{ 38, 38, 43, alpha };
			const auto border = xdraw::color{ 55, 55, 62, alpha };
			const auto accent_a = accent.alpha( alpha );

			draw_list.rect_filled( x, y, w, h, body, xdraw::corner_radius{ rounding } );
			draw_list.rect( x, y, w, h, border, xdraw::corner_radius{ rounding }, 1.0f );
			draw_list.rect_filled( x + 1.0f, y + 1.0f, w - 2.0f, 3.0f, accent_a, xdraw::corner_radius::top( rounding - 1.0f ) );
		}

		inline void draw_arc( xdraw::draw_list& draw_list, float cx, float cy, float radius, float start, float end, const xdraw::color& col, float thickness )
		{
			if ( end <= start )
			{
				return;
			}

			constexpr auto segments{ 32 };
			std::vector<float> pts;
			pts.reserve( static_cast< std::size_t >( segments + 1 ) * 2 );

			for ( auto i = 0; i <= segments; ++i )
			{
				const auto t = start + ( end - start ) * ( static_cast< float >( i ) / static_cast< float >( segments ) );
				pts.push_back( cx + std::cos( t ) * radius );
				pts.push_back( cy + std::sin( t ) * radius );
			}

			draw_list.polyline( pts, col, false, thickness );
		}

	}

	void overlay::on_render( xdraw::draw_list& draw_list )
	{
		if ( gui )
		{
			gui->easing(
				this->m_bomb_appear_alpha,
				settings::g_esp.m_other.bomb_timer.value ? 1.0f : 0.0f,
				menu_motion::k_menu_open,
				dynamic_easing );
		}
		else
		{
			this->m_bomb_appear_alpha = settings::g_esp.m_other.bomb_timer.value ? 1.0f : 0.0f;
		}

		if ( this->m_bomb_appear_alpha > 0.01f )
		{
			this->add_bomb( draw_list, this->m_bomb_appear_alpha );
		}

		if ( settings::g_esp.m_other.m_chicken.enabled.value )
		{
			this->add_chickens( draw_list );
		}
		else
		{
			this->m_fade_alpha.clear( );
		}
	}

	void overlay::add_bomb( xdraw::draw_list& draw_list, float appear_alpha )
	{
		const auto menu_open = rendering::g_menu.is_open( );
		const auto local = systems::g_local.get( );
		const auto planted_c4 = memory::read<std::uintptr_t>( addresses::globals::planted_c4 );
		const auto global_vars = memory::read<std::uintptr_t>( addresses::globals::global_vars );

		auto preview{ false };
		auto damage{ 0 };
		auto time_left{ 0.0f };
		auto bomb_site{ 0 };
		auto being_defused{ false };
		auto timer_length{ 40.0f };
		auto progress{ 0.5f };

		if ( planted_c4 && global_vars )
		{
			const auto current_time = memory::read<float>( global_vars + 0x30 );
			const auto blow_time = reinterpret_cast<C_PlantedC4*>( planted_c4 )->m_flC4Blow( );
			const auto has_exploded = reinterpret_cast<C_PlantedC4*>( planted_c4 )->m_bHasExploded( );
			const auto bomb_defused = reinterpret_cast<C_PlantedC4*>( planted_c4 )->m_bBombDefused( );

			time_left = blow_time - current_time;

			if ( bomb_defused || ( has_exploded && time_left < -2.0f ) || time_left <= 0.0f )
			{
				if ( !menu_open )
				{
					return;
				}

				preview = true;
			}
			else
			{
				bomb_site = reinterpret_cast<C_PlantedC4*>( planted_c4 )->m_nBombSite( );
				being_defused = reinterpret_cast<C_PlantedC4*>( planted_c4 )->m_bBeingDefused( );
				timer_length = reinterpret_cast<C_PlantedC4*>( planted_c4 )->m_flTimerLength( );
				progress = timer_length > 0.1f ? std::clamp( time_left / timer_length, 0.0f, 1.0f ) : 0.0f;

				const auto view_pawn = local.view_pawn( );
				if ( memory::is_game_ptr( view_pawn ) )
				{
					const auto c4_scene_node = reinterpret_cast<C_BaseEntity*>( planted_c4 )->m_pGameSceneNode( );
					const auto pawn_scene_node = reinterpret_cast<C_BaseEntity*>( view_pawn )->m_pGameSceneNode( );

					if ( memory::is_game_ptr( c4_scene_node ) && memory::is_game_ptr( pawn_scene_node ) )
					{
						const auto c4_origin = reinterpret_cast<CGameSceneNode*>( c4_scene_node )->m_vecAbsOrigin( );
						const auto pawn_origin = reinterpret_cast<CGameSceneNode*>( pawn_scene_node )->m_vecAbsOrigin( );
						const auto distance = ( c4_origin - pawn_origin ).length( );

						constexpr auto default_damage{ 650.0f };
						constexpr auto default_radius{ 2275.0f };
						const auto sigma = default_radius / 3.0f;
						auto dmg = default_damage * std::exp( -( distance * distance ) / ( 2.0f * sigma * sigma ) );

						const auto armor = reinterpret_cast<C_CSPlayerPawn*>( view_pawn )->m_ArmorValue( );
						if ( armor > 0 )
						{
							constexpr auto armor_ratio = 0.5f;
							constexpr auto armor_bonus = 0.5f;
							auto armor_absorbed = dmg * armor_ratio;
							auto armor_cost = ( dmg - armor_absorbed ) * armor_bonus;
							if ( armor_cost > static_cast< float >( armor ) )
							{
								armor_cost = static_cast< float >( armor ) * ( 1.0f / armor_bonus );
								armor_absorbed = dmg - armor_cost;
							}
							dmg = armor_absorbed;
						}

						damage = static_cast< int >( std::floor( dmg ) );
					}
				}
			}
		}
		else
		{
			if ( !menu_open )
			{
				return;
			}

			preview = true;
		}

		if ( preview )
		{
			damage = 50;
			time_left = 10.0f;
			bomb_site = 0;
			progress = 0.5f;
			being_defused = false;
		}

		( void )draw_list;

		const auto view_pawn = local.view_pawn( );
		const auto health = memory::is_game_ptr( view_pawn ) ? reinterpret_cast<C_BaseEntity*>( view_pawn )->m_iHealth( ) : 0;
		const auto lethal = !preview && damage > 0 && health > 0 && damage >= health;

		auto* fg = ImGui::GetForegroundDrawList( );
		if ( !fg ) return;

		auto* text_font = font ? font->get( main_font_data, 13.0f ) : nullptr;
		auto* icon_font = font ? font->get( main_font_data, 14.0f ) : nullptr;
		if ( !text_font || !icon_font || !text_font->IsLoaded( ) ) return;

		const auto& io = ImGui::GetIO( );
		const auto screen_w = io.DisplaySize.x;
		const auto screen_h = io.DisplaySize.y;

		const float header_h = SCALE( 28.0f );
		const float row_h = SCALE( 22.0f );
		const int rows = 3;
		const float width = SCALE( 210.0f );
		const float total_h = header_h + rows * row_h + SCALE( 8.0f );
		const float pad = SCALE( 12.0f );

		auto& pos_x = settings::g_esp.m_other.bomb_pos_x.value;
		auto& pos_y = settings::g_esp.m_other.bomb_pos_y.value;
		if ( pos_x < 0.0f || pos_y < 0.0f )
		{
			pos_x = screen_w * 0.5f - width * 0.5f;
			pos_y = screen_h * 0.18f;
		}

		if ( menu_open )
		{
			const auto mx = io.MousePos.x;
			const auto my = io.MousePos.y;
			const auto panel_hovered = mx >= pos_x && mx <= pos_x + width && my >= pos_y && my <= pos_y + total_h;

			static auto dragging{ false };
			static float drag_ox{}, drag_oy{};

		if ( panel_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
			{
				dragging = true;
				drag_ox = mx - pos_x;
				drag_oy = my - pos_y;
			}
			if ( dragging )
			{
			if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
				{
					pos_x = std::clamp( mx - drag_ox, 0.0f, screen_w - width );
					pos_y = std::clamp( my - drag_oy, 0.0f, screen_h - total_h );
				}
				else dragging = false;
			}
		}

		const float slide = SCALE( 8.0f ) * ( 1.0f - appear_alpha );
		const ImVec2 p0( pos_x, pos_y + slide );
		const ImVec2 p1( pos_x + width, pos_y + slide + total_h );
		const ImVec2 hdr_p1( pos_x + width, pos_y + slide + header_h );

		auto fade = [ appear_alpha ]( ImU32 c ) -> ImU32
		{
			const int a = static_cast< int >( ( ( c >> IM_COL32_A_SHIFT ) & 0xFF ) * appear_alpha + 0.5f );
			return ( c & ~IM_COL32_A_MASK ) | ( static_cast< ImU32 >( a ) << IM_COL32_A_SHIFT );
		};

		const float shadow_blur = SCALE( 12.0f );
		for ( int s = 0; s < 6; ++s )
		{
			const float t = static_cast<float>( s ) / 6.0f;
			const float a = ( 1.0f - t ) * 0.25f * appear_alpha;
			const float bl = shadow_blur * t;
			fg->AddRectFilled( ImVec2( p0.x - bl, p0.y - bl ), ImVec2( p1.x + bl, p1.y + bl ),
				IM_COL32( 0, 0, 0, static_cast<int>( a * 255 ) ), SCALE( 6.0f ) + bl );
		}

		fg->AddRectFilled( p0, p1, fade( IM_COL32( 31, 31, 36, 242 ) ), SCALE( 6.0f ) );
		fg->AddRect( p0, p1, fade( IM_COL32( 0, 0, 0, 204 ) ), SCALE( 6.0f ), 0, SCALE( 1.5f ) );
		fg->AddRectFilled( p0, hdr_p1, fade( IM_COL32( 35, 35, 40, 242 ) ), SCALE( 6.0f ), ImDrawFlags_RoundCornersTop );
		fg->AddRect( p0, hdr_p1, fade( IM_COL32( 0, 0, 0, 153 ) ), SCALE( 6.0f ), ImDrawFlags_RoundCornersTop, SCALE( 1.0f ) );

		const char* hdr_icon = "\xEF\x87\xA2";
		const ImVec2 hdr_icon_sz = icon_font->CalcTextSizeA( 14.0f, FLT_MAX, 0, hdr_icon );
		fg->AddText( icon_font, 14.0f,
			ImVec2( pos_x + SCALE( 10.0f ), p0.y + ( header_h - hdr_icon_sz.y ) * 0.5f ),
			fade( IM_COL32( 179, 143, 228, 255 ) ), hdr_icon );

		const char* hdr_text = preview ? "bomb (preview)" : "bomb";
		const ImVec2 hdr_text_sz = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0, hdr_text );
		fg->AddText( text_font, 13.0f,
			ImVec2( pos_x + SCALE( 10.0f ) + hdr_icon_sz.x + SCALE( 6.0f ), p0.y + ( header_h - hdr_text_sz.y ) * 0.5f ),
			fade( IM_COL32( 230, 230, 230, 255 ) ), hdr_text );

		const auto accent = fade( IM_COL32( 179, 143, 228, 255 ) );
		const auto label_col = fade( IM_COL32( 155, 155, 165, 255 ) );
		const auto value_col = fade( IM_COL32( 235, 235, 240, 255 ) );
		const auto lethal_col = fade( IM_COL32( 235, 90, 90, 255 ) );

		float row_y = p0.y + header_h + SCALE( 4.0f );

		auto draw_row = [ & ]( const char* label, const char* value, ImU32 vcol ) {
			const ImVec2 lsz = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0, label );
			const ImVec2 vsz = text_font->CalcTextSizeA( 13.0f, FLT_MAX, 0, value );
			fg->AddText( text_font, 13.0f,
				ImVec2( pos_x + pad, row_y + ( row_h - lsz.y ) * 0.5f ),
				label_col, label );
			fg->AddText( text_font, 13.0f,
				ImVec2( pos_x + width - pad - vsz.x, row_y + ( row_h - vsz.y ) * 0.5f ),
				vcol, value );
			row_y += row_h;
		};

		char site_val[ 8 ]{}; std::snprintf( site_val, sizeof( site_val ), "%c", bomb_site == 0 ? 'A' : 'B' );
		char time_val[ 16 ]{}; std::snprintf( time_val, sizeof( time_val ), "%.1fs", time_left );
		char dmg_val[ 16 ]{};
		if ( being_defused ) std::snprintf( dmg_val, sizeof( dmg_val ), "defusing" );
		else if ( lethal ) std::snprintf( dmg_val, sizeof( dmg_val ), "lethal (%d)", damage );
		else std::snprintf( dmg_val, sizeof( dmg_val ), "%d hp", damage );

		draw_row( "site", site_val, accent );
		draw_row( "time", time_val, value_col );
		draw_row( "damage", dmg_val, lethal ? lethal_col : value_col );

		( void )progress;
		( void )timer_length;
	}

	void overlay::add_spectators( xdraw::draw_list& draw_list )
	{
		if ( !settings::g_esp.m_other.spectator_list.value )
		{
			return;
		}

		const auto menu_open = rendering::g_menu.is_open( );
		const auto local = systems::g_local.get( );
		const auto in_game = local.is_valid( ) && systems::g_entities.exists( local.controller );

		struct spectator_entry
		{
			std::string name{};
			std::uintptr_t steam_id{};
		};

		std::vector<spectator_entry> entries{};

		if ( in_game )
		{
			const auto game_rules = memory::read<std::uintptr_t>( addresses::globals::game_rules );
			if ( memory::is_game_ptr( game_rules ) && reinterpret_cast<C_CSGameRules*>( game_rules )->m_gamePhase( ) < 4 )
			{
				const auto local_controller = local.controller;
				const auto spectator_target = systems::g_entities.observer_target(
					systems::g_entities.observer_pawn( local_controller ) );
				const auto local_is_spectator = memory::is_game_ptr( spectator_target );
				const auto watched_pawn = local_is_spectator ? spectator_target : local.view_pawn( );
				if ( memory::is_game_ptr( watched_pawn ) )
				{
					auto push_unique = [ & ]( std::string name, std::uintptr_t steam_id )
					{
						if ( name.empty( ) )
						{
							return;
						}

						std::ranges::transform( name, name.begin( ), [ ]( unsigned char c ) { return static_cast< char >( std::tolower( c ) ); } );

						for ( const auto& existing : entries )
						{
							if ( existing.name == name )
							{
								return;
							}
						}

						entries.push_back( { std::move( name ), steam_id } );
					};

					for ( const auto& player : systems::g_entities.get_by_type( systems::entities::type::player ) )
					{
						if ( player.ptr == local_controller || !memory::is_game_ptr( player.ptr ) )
						{
							continue;
						}

						const auto observer_target = systems::g_entities.observer_target(
							systems::g_entities.observer_pawn( player.ptr ) );
						if ( observer_target != watched_pawn )
						{
							continue;
						}

						const auto name_ptr = reinterpret_cast<CCSPlayerController*>( player.ptr )->m_sSanitizedPlayerName( );
						if ( !name_ptr )
						{
							continue;
						}

						push_unique(
							memory::read_string( name_ptr, 127 ),
							reinterpret_cast<CBasePlayerController*>( player.ptr )->m_steamID( ) );
					}

					if ( local_is_spectator )
					{
						const auto name_ptr = reinterpret_cast<CCSPlayerController*>( local_controller )->m_sSanitizedPlayerName( );
						push_unique(
							name_ptr ? memory::read_string( name_ptr, 127 ) : std::string{ "you" },
							reinterpret_cast<CBasePlayerController*>( local_controller )->m_steamID( ) );
					}

					std::ranges::sort( entries, {}, &spectator_entry::name );
				}
			}
		}

		const auto has_entries = !entries.empty( );
		if ( !has_entries && !menu_open )
		{
			return;
		}

		const auto [screen_w, screen_h] = xdraw::viewport_size( );
		const auto& style = xui::ctx( ).style;

		constexpr auto rounding{ 4.0f };
		constexpr auto accent_h{ 4.0f };
		constexpr auto header_h{ 25.0f };
		constexpr auto item_h{ 24.0f };
		constexpr auto padding_x{ 12.0f };
		constexpr auto base_width{ 150.0f };
		constexpr auto empty_content_h{ 10.0f };

		auto panel_w = base_width;
		for ( const auto& e : entries )
		{
			const auto [nw, _] = xdraw::measure_text( e.name );
			panel_w = std::max( panel_w, nw + padding_x * 2.0f );
		}

		const auto content_h = has_entries ? static_cast< float >( entries.size( ) ) * item_h : empty_content_h;
		const auto panel_h = header_h + content_h;

		static float pos_x{ -1.0f };
		static float pos_y{ -1.0f };
		if ( pos_x < 0.0f || pos_y < 0.0f )
		{
			pos_x = static_cast< float >( screen_w ) - 200.0f;
			pos_y = static_cast< float >( screen_h ) * 0.55f;
		}

		if ( menu_open )
		{
			const auto& io = ImGui::GetIO( );
			const auto mx = io.MousePos.x;
			const auto my = io.MousePos.y;
			const auto hovered = mx >= pos_x && mx <= pos_x + panel_w && my >= pos_y && my <= pos_y + panel_h;

			static auto dragging{ false };
			static float drag_ox{}, drag_oy{};

		if ( hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
			{
				dragging = true;
				drag_ox = mx - pos_x;
				drag_oy = my - pos_y;
			}

			if ( dragging )
			{
			if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
				{
					pos_x = std::clamp( mx - drag_ox, 0.0f, static_cast< float >( screen_w ) - panel_w );
					pos_y = std::clamp( my - drag_oy, 0.0f, static_cast< float >( screen_h ) - panel_h );
				}
				else
				{
					dragging = false;
				}
			}
		}

		const auto alpha = has_entries ? static_cast< std::uint8_t >( 255 ) : static_cast< std::uint8_t >( 200 );
		const auto header_bg = xdraw::color{ 30, 30, 30, alpha };
		const auto content_bg = xdraw::color{ 29, 29, 29, static_cast< std::uint8_t >( ( 150 * alpha ) / 255 ) };
		const auto accent = style.accent.alpha( alpha );
		const auto text_col = xdraw::color{ 255, 255, 255, alpha };

		draw_list.rect_filled( pos_x, pos_y - 1.0f, panel_w, accent_h + 1.0f, accent, xdraw::corner_radius::top( rounding ) );
		draw_list.rect_filled( pos_x, pos_y, panel_w, header_h, header_bg, xdraw::corner_radius::top( rounding ) );

		const auto [title_w, title_h] = xdraw::measure_text( "spectators" );
		draw_list.text(
			pos_x + ( panel_w - title_w ) * 0.5f,
			pos_y + ( header_h - title_h ) * 0.5f,
			"spectators",
			text_col );

		const auto content_y = pos_y + header_h;
		draw_list.rect_filled( pos_x, content_y, panel_w, content_h, content_bg, xdraw::corner_radius::bottom( rounding ) );

		if ( !has_entries )
		{
			return;
		}

		auto row_y = content_y;
		for ( const auto& e : entries )
		{
			auto text_x = pos_x + padding_x - 2.0f;

			const auto [nw, nh] = xdraw::measure_text( e.name );
			draw_list.text( text_x, row_y + ( item_h - nh ) * 0.5f, e.name, text_col );
			row_y += item_h;
		}
	}

	void overlay::add_chickens( xdraw::draw_list& draw_list )
	{
		const auto& cfg = settings::g_esp.m_other.m_chicken;

		std::unordered_set<std::uintptr_t> seen_this_frame;
		seen_this_frame.reserve( this->m_fade_alpha.size( ) + 8 );

		const auto now = std::chrono::steady_clock::now( );
		const auto delta_s = this->m_last_tick.time_since_epoch( ).count( ) > 0
			? std::chrono::duration<float>( now - this->m_last_tick ).count( )
			: 0.0f;
		this->m_last_tick = now;
		const auto fade_speed = 6.5f;

		math::vector3 dist_origin = systems::g_view.origin( );
		if ( const auto local = systems::g_local.get( ); local.is_valid( ) )
		{
			if ( const auto local_node = reinterpret_cast<C_BaseEntity*>( local.pawn )->m_pGameSceneNode( ) )
				dist_origin = reinterpret_cast<CGameSceneNode*>( local_node )->m_vecAbsOrigin( );
		}

		for ( const auto& entity : systems::g_entities.get_by_type( systems::entities::type::chicken ) )
		{
			if ( !entity.ptr )
			{
				continue;
			}

			const auto game_scene_node = reinterpret_cast<C_BaseEntity*>( entity.ptr )->m_pGameSceneNode( );
			if ( !game_scene_node )
			{
				continue;
			}

			if ( reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_bDormant( ) )
			{
				continue;
			}

			const auto origin = reinterpret_cast<CGameSceneNode*>( game_scene_node )->m_vecAbsOrigin( );
			if ( !std::isfinite( origin.x ) || !std::isfinite( origin.y ) || !std::isfinite( origin.z ) )
			{
				continue;
			}

			const auto distance = dist_origin.distance( origin );
			const auto max_dist = cfg.max_distance.value;
			float target_alpha = 1.0f;
			if ( max_dist > 0.0f )
			{
				const auto fade_start = max_dist * 0.85f;
				if ( distance >= max_dist )
					target_alpha = 0.0f;
				else if ( distance > fade_start )
					target_alpha = 1.0f - ( distance - fade_start ) / ( max_dist - fade_start );
			}

			auto& state = this->m_fade_alpha[ entity.ptr ];
			state = state + ( target_alpha - state ) * std::min( 1.0f, delta_s * fade_speed );
			seen_this_frame.insert( entity.ptr );

			if ( state <= 0.01f )
			{
				continue;
			}

			auto bounds = systems::g_bounds.get( entity.ptr );
			if ( !bounds.valid )
			{
				auto head = origin;
				head.z += 18.0f;

				const auto feet = systems::g_view.project( origin );
				const auto head_screen = systems::g_view.project( head );
				if ( !systems::g_view.projection_valid( feet ) || !systems::g_view.projection_valid( head_screen ) )
				{
					continue;
				}

				bounds.min = { std::min( feet.x, head_screen.x ) - 8.0f, std::min( feet.y, head_screen.y ) };
				bounds.max = { std::max( feet.x, head_screen.x ) + 8.0f, std::max( feet.y, head_screen.y ) };
				bounds.valid = true;
			}

			auto color = cfg.color.value;
			color.a = static_cast<std::uint8_t>( std::clamp( static_cast<float>( color.a ) * state, 0.0f, 255.0f ) );

			const auto x = std::floorf( bounds.min.x );
			const auto y = std::floorf( bounds.min.y );
			const auto w = std::floorf( bounds.width( ) );
			const auto h = std::floorf( bounds.height( ) );

			if ( cfg.box.value && w >= 1.0f && h >= 1.0f )
			{
				const auto outline = xdraw::color{ 0, 0, 0, color.a };
				draw_list.rect( x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f, outline, 1.0f );
				draw_list.rect( x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, outline, 1.0f );
				draw_list.rect( x, y, w, h, color, 1.0f );
			}

			if ( !cfg.name.value && !cfg.distance.value )
			{
				continue;
			}

			std::string label{};
			if ( cfg.name.value && cfg.distance.value )
			{
				label = std::format( "chicken {:.0f}m", distance );
			}
			else if ( cfg.name.value )
			{
				label = "chicken";
			}
			else
			{
				label = std::format( "{:.0f}m", distance );
			}

			const auto text_size = detail::measure_esp_text( label );
			const auto text_x = std::floorf( x + ( w * 0.5f ) - ( text_size.x * 0.5f ) );
			const auto text_y = std::floorf( y - text_size.y - 2.0f );
			detail::draw_esp_text( draw_list, text_x, text_y, label, color, true );
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

}

