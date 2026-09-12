#include "headers/includes.h"
#include "data/IconsFontAwesome6.h"
#include "data/IconsV2_Defines.h"
#include "data/images.h"
#include "data/shaders/shader_manager.h"
#include "data/shaders/aurora_glow.h"
#include <core/menu/rendering.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

	struct preview_group_selector_anim_t
	{
		float active[ 3 ]{ 1.0f, 0.0f, 0.0f };
		float hover[ 3 ]{ 0.0f, 0.0f, 0.0f };
	};

	void draw_preview_group_selector( int* group )
	{
		if ( !group || !gui || !draw || !font )
			return;

		ImGuiWindow* window = ImGui::GetCurrentWindow( );
		if ( !window )
			return;

		ImFont* icon_font = font->get( main_font_data, menu_typography::k_control );
		ImFont* text_font = font->get( main_font_data, menu_typography::k_control );
		if ( !icon_font || !text_font )
			return;

		const int values[ 3 ]{ 0, 2, 1 };
		const char* labels[ 3 ]{ "Enemy", "Local", "Team" };
		const char* icons[ 3 ]{ ICON_FA_SKULL, ICON_FA_USER, ICON_FA_USER_GROUP };
		const float height = SCALE( 22.0f );
		const float collapsed_w = SCALE( 30.0f );
		const float expanded_w = SCALE( 88.0f );
		const float gap = SCALE( 6.0f );
		const float rounding = SCALE( 8.0f );
		const ImVec2 start = ImGui::GetCursorScreenPos( );
		const ImVec2 mouse = ImGui::GetMousePos( );

		preview_group_selector_anim_t* anim =
			gui->anim_container<preview_group_selector_anim_t>( window->GetID( "##preview_group_selector" ) );
		if ( !anim )
			return;

		float x = start.x;
		for ( int i = 0; i < 3; ++i )
		{
			const bool selected = *group == values[ i ];
			gui->easing( anim->active[ i ], selected ? 1.0f : 0.0f, 12.0f, dynamic_easing );
			const float width = collapsed_w + ( expanded_w - collapsed_w ) * anim->active[ i ];
			const ImRect rect( ImVec2( x, start.y ), ImVec2( x + width, start.y + height ) );
			const bool hovered = rect.Contains( mouse );
			gui->easing( anim->hover[ i ], hovered ? 1.0f : 0.0f, 16.0f, dynamic_easing );

			ImGui::SetCursorScreenPos( ImVec2( x, start.y ) );
			ImGui::PushID( i );
			ImGui::InvisibleButton( "##preview_group", ImVec2( width, height ) );
			if ( ImGui::IsItemClicked( ImGuiMouseButton_Left ) || ( hovered && gui->mouse_clicked( mouse_button_left ) ) )
				*group = values[ i ];
			ImGui::PopID( );

			const ImVec4 bg = selected
				? ImVec4( 0.22f, 0.21f, 0.25f, 0.98f )
				: hovered
				? ImVec4( 0.18f, 0.18f, 0.20f, 0.98f )
				: ImVec4( 0.15f, 0.15f, 0.17f, 0.96f );
			const ImVec4 border = selected
				? ImVec4( 0.36f, 0.36f, 0.40f, 0.95f )
				: ImVec4( 0.28f, 0.28f, 0.31f, 0.90f );
			const ImVec4 accent = ImVec4( 0.79f, 0.74f, 0.88f, 1.0f );
			const ImVec4 text = selected
				? ImVec4( 0.94f, 0.94f, 0.96f, 1.0f )
				: ImVec4( 0.80f, 0.80f, 0.84f, 0.96f );

			draw->rect_filled( gui->window_drawlist( ), rect.Min, rect.Max, draw->get_clr( bg ), rounding );
			draw->rect(
				gui->window_drawlist( ),
				ImVec2( rect.Min.x + 0.5f, rect.Min.y + 0.5f ),
				ImVec2( rect.Max.x - 0.5f, rect.Max.y - 0.5f ),
				draw->get_clr( border ),
				rounding,
				0,
				SCALE( 1.0f ) );

			if ( selected || anim->hover[ i ] > 0.01f )
				draw->rect_filled( gui->window_drawlist( ), rect.Min, rect.Max, draw->get_clr( accent, selected ? 0.10f : 0.05f * anim->hover[ i ] ), rounding );

			const ImVec2 icon_size = icon_font->CalcTextSizeA( icon_font->FontSize, FLT_MAX, 0.0f, icons[ i ] );
			const float centered_icon_x = rect.Min.x + ( width - icon_size.x ) * 0.5f;
			const float aligned_icon_x = rect.Min.x + SCALE( 9.0f );
			const float icon_x = centered_icon_x + ( aligned_icon_x - centered_icon_x ) * anim->active[ i ];
			const ImVec2 icon_pos( icon_x, rect.Min.y + ( height - icon_size.y ) * 0.5f );
			draw->text( gui->window_drawlist( ), icon_font, icon_font->FontSize, icon_pos, draw->get_clr( selected ? accent : text ), icons[ i ] );

			if ( anim->active[ i ] > 0.01f )
			{
				const ImVec2 text_size = text_font->CalcTextSizeA( menu_typography::k_control, FLT_MAX, 0.0f, labels[ i ] );
				const ImVec2 text_pos(
					icon_pos.x + icon_size.x + SCALE( 7.0f ),
					rect.Min.y + ( height - text_size.y ) * 0.5f );
				draw->text( gui->window_drawlist( ), text_font, menu_typography::k_control, text_pos, draw->get_clr( text, anim->active[ i ] ), labels[ i ] );
			}

			x += width + gap;
		}
	}

	struct menu_search_entry
	{
		const char* label;
		int tab;
		int subtab;
		const char* focus = nullptr;
	};

	constexpr menu_search_entry k_search_entries[] = {
		{ "box", 0, 0 }, { "skeleton", 0, 0 }, { "healthbar", 0, 0 }, { "health bar", 0, 0, "healthbar" },
		{ "ammobar", 0, 0 }, { "ammo bar", 0, 0, "ammobar" }, { "name", 0, 0 }, { "weapon", 0, 0 }, { "flags", 0, 0 },
		{ "player esp", 0, 0, "box" }, { "offscreen arrows", 0, 0 }, { "preview team", 0, 0 },
		{ "ct model", 0, 0 }, { "t model", 0, 0 }, { "preview weapon", 0, 0 },
		{ "glow", 0, 0 }, { "ragdoll glow", 0, 0 }, { "player chams", 0, 0 }, { "ragdoll chams", 0, 0 },
		{ "backtrack chams", 0, 0 }, { "onshot chams", 0, 0 }, { "onshot trail", 0, 0 },
		{ "local glow", 0, 0 }, { "local chams", 0, 0 }, { "local ragdoll", 0, 0 },
		{ "weapon chams", 0, 0 }, { "arms chams", 0, 0 }, { "gloves chams", 0, 0 },
		{ "item esp", 0, 1 }, { "projectile esp", 0, 1 }, { "bomb timer", 0, 1 }, { "keybinds", 0, 1 },
		{ "spotify", 0, 1 }, { "spectators", 0, 1 }, { "spectator list", 0, 1, "spectators" }, { "chicken", 0, 1 },
		{ "custom skybox", 0, 1 }, { "skybox", 0, 1, "custom skybox" }, { "world modulation", 0, 1 },
		{ "world material", 0, 1 }, { "lighting", 0, 1 }, { "bloom", 0, 1 }, { "gamma", 0, 1 },
		{ "depth of field", 0, 1 }, { "night mode", 0, 1 }, { "fullbright", 0, 1 }, { "weather", 0, 1 },
		{ "fog", 0, 1 }, { "wetness", 0, 1 }, { "modulation", 0, 1 }, { "ground particles", 0, 1 },
		{ "hit particles", 0, 1 }, { "kill particles", 0, 1 }, { "throwable trail", 0, 1 },
		{ "change fov", 0, 2 }, { "scoped fov override", 0, 2 }, { "aspect ratio", 0, 2 }, { "thirdperson", 0, 2 },
		{ "local alpha", 0, 2 }, { "freecam", 0, 2 }, { "viewmodel", 0, 2 },
		{ "viewmodel offset", 0, 2, "viewmodel" }, { "crosshair", 0, 2 }, { "scope overlay", 0, 2 },
		{ "bullet boxes", 0, 3 }, { "bullet tracers", 0, 3 }, { "bullet effects", 0, 3 },
		{ "grenade prediction", 0, 3 }, { "grenade proximity", 0, 3 }, { "molotov area", 0, 3 }, { "dlight", 0, 3 },
		{ "legit", 1, 0 }, { "aimbot", 1, 0 }, { "visualize fov", 1, 0 }, { "rcs", 1, 0 }, { "triggerbot", 1, 0 },
		{ "rage", 1, 1 }, { "silent", 1, 1 }, { "no spread", 1, 1 }, { "body aim", 1, 1 }, { "prefer safe point", 1, 1 },
		{ "baim on low hitchance", 1, 1 }, { "dynamic pointscale", 1, 1 }, { "hitchance override", 1, 1 },
		{ "damage override", 1, 1 }, { "force shot", 1, 1 }, { "auto revolver", 1, 1 }, { "auto scope", 1, 1 },
		{ "auto stop", 1, 1 }, { "extrapolation", 1, 1 }, { "zeusbot", 1, 1 }, { "knifebot", 1, 1 },
		{ "quickpeek", 1, 1 }, { "quick peek", 1, 1, "quickpeek" }, { "duckpeek", 1, 1 },
		{ "duck peek", 1, 1, "duckpeek" },
		{ "anti-aim", 1, 2 }, { "anti aim", 1, 2, "anti-aim" }, { "at target", 1, 2 }, { "compensate roll", 1, 2 },
		{ "hide shots", 1, 2 }, { "avoid backstab", 1, 2 }, { "spin", 1, 2 }, { "yaw jitter", 1, 2 },
		{ "pitch jitter", 1, 2 }, { "manual left", 1, 2 }, { "manual right", 1, 2 }, { "mouse override", 1, 2 },
		{ "direction indicator", 1, 2 },
		{ "bhop", 2, 0 }, { "bunny hop", 2, 0, "bhop" }, { "airstrafe", 2, 0 }, { "auto strafe", 2, 0, "airstrafe" },
		{ "fully directional", 2, 0 }, { "subtick strafe", 2, 0 }, { "fastladder", 2, 0 },
		{ "hit log", 2, 1 }, { "miss log", 2, 1 }, { "chat log", 2, 1 }, { "hit marker", 2, 1 },
		{ "hit sound", 2, 1 }, { "death sound", 2, 1 }, { "hit effect", 2, 1 }, { "death effect", 2, 1 },
		{ "reveal radar", 2, 1 }, { "reveal scoreboard weapons", 2, 1 }, { "preserve killfeed", 2, 1 },
		{ "enemy spectate", 2, 1 }, { "spectate thirdperson", 2, 1 }, { "disable game logs", 2, 1 },
		{ "clantag", 2, 1 }, { "name override", 2, 1 }, { "autobuy", 2, 1 },
		{ "force crosshair", 2, 2 }, { "scope", 2, 2 }, { "remove scope", 2, 2, "scope" }, { "overhead", 2, 2 },
		{ "legs", 2, 2 }, { "recoil", 2, 2 }, { "remove recoil", 2, 2, "recoil" }, { "skybox fog", 2, 2 },
		{ "skybox 3d", 2, 2 }, { "decals", 2, 2 }, { "smoke", 2, 2 }, { "remove smoke", 2, 2, "smoke" },
		{ "flash alpha", 2, 2 }, { "remove flash", 2, 2, "flash alpha" },
		{ "skins", 3, 0 }, { "skin search", 3, 0 },
		{ "config", 4, 0 }, { "cloud configs", 4, 0, "config" }
	};

	std::string lower_copy( std::string_view text )
	{
		std::string out;
		out.reserve( text.size( ) );
		for ( const unsigned char ch : text )
			out.push_back( static_cast< char >( std::tolower( ch ) ) );
		return out;
	}

	bool contains_insensitive( std::string_view haystack, std::string_view needle )
	{
		if ( needle.empty( ) )
			return true;

		const std::string lower_haystack = lower_copy( haystack );
		const std::string lower_needle = lower_copy( needle );
		return lower_haystack.find( lower_needle ) != std::string::npos;
	}

	int search_match_rank( std::string_view label, std::string_view query )
	{
		const std::string lower_label = lower_copy( label );
		const std::string lower_query = lower_copy( query );
		if ( lower_label == lower_query )
			return 0;
		if ( lower_label.rfind( lower_query, 0 ) == 0 )
			return 1;
		if ( lower_label.find( lower_query ) != std::string::npos )
			return 2;
		return 3;
	}

	void navigate_to_search_result( const menu_search_entry& entry )
	{
		menu_interaction::close_popups_for_navigation( );
		var->gui.active_tab = entry.tab;
		var->gui.active_subtab = entry.subtab;
		var->gui.tab_content_anim = 0.0f;
		var->gui.searchbar_active = false;
		var->gui.search_hovered_result = -1;
		var->gui.search_navigation_found = false;
		var->gui.search_navigation_pending = true;
		var->gui.search_navigate_to = entry.focus ? entry.focus : entry.label;
		var->gui.search_buffer[ 0 ] = '\0';
	}

}
void c_gui::render()
{
	var->gui.popup_blocks_input_prev = var->gui.popup_blocks_input;
	var->gui.dropdown_blocks_input_prev = var->gui.dropdown_blocks_input;
	var->gui.color_picker_open_prev = var->gui.color_picker_open;
	var->gui.bind_popup_visible_prev = var->gui.bind_popup_visible;
	var->gui.overlay_blocks_welcome = false;
	var->gui.popup_blocks_input = false;
	var->gui.dropdown_blocks_input = false;
	var->gui.color_picker_open = false;
	var->gui.bind_popup_visible = false;

	gui->easing(var->gui.watermark_spawn_alpha,      var->gui.show_watermark       ? 1.0f : 0.0f, 8.0f, dynamic_easing);
	gui->easing(var->gui.active_hotkeys_spawn_alpha, var->gui.show_active_hotkeys  ? 1.0f : 0.0f, 8.0f, dynamic_easing);
	gui->easing(var->gui.spotify_spawn_alpha,        var->gui.show_spotify         ? 1.0f : 0.0f, 8.0f, dynamic_easing);
	gui->easing(var->gui.playerlist_spawn_alpha,     var->gui.show_playerlist      ? 1.0f : 0.0f, 8.0f, dynamic_easing);
	gui->easing(var->gui.spectator_list_spawn_alpha, var->gui.show_spectator_list  ? 1.0f : 0.0f, 8.0f, dynamic_easing);

	
	const bool menu_target_open = var->gui.menu_open;
	static bool previous_menu_target_open = true;
	if (!menu_target_open && previous_menu_target_open)
		menu_interaction::close_popups_for_navigation();
	previous_menu_target_open = menu_target_open;

	if (menu_target_open && var->gui.menu_was_closed)
	{
		var->gui.menu_open_alpha = 0.0f;
		var->gui.esp_preview_alpha = 0.0f;
		var->gui.esp_feature_panel_alpha = 0.0f;
		var->gui.menu_was_closed = false;
	}

	gui->easing(var->gui.menu_open_alpha, menu_target_open ? 1.0f : 0.0f, menu_motion::k_menu_open, dynamic_easing);

	const bool on_players_preview_tab = var->gui.active_tab == 0 && var->gui.active_subtab == 0;
	const bool overlay_preview_group = var->gui.esp_preview_group >= 0 && var->gui.esp_preview_group <= 2;
	gui->easing(
		var->gui.esp_preview_alpha,
		(menu_target_open && on_players_preview_tab) ? 1.0f : 0.0f,
		menu_motion::k_menu_open,
		dynamic_easing);
	gui->easing(
		var->gui.esp_feature_panel_alpha,
		(menu_target_open && on_players_preview_tab && overlay_preview_group && var->gui.side_panel_open) ? 1.0f : 0.0f,
		menu_motion::k_menu_open,
		dynamic_easing);

	var->gui.menu_render_visible =
		menu_target_open ||
		var->gui.menu_open_alpha > 0.01f ||
		var->gui.esp_preview_alpha > 0.01f ||
		var->gui.esp_feature_panel_alpha > 0.01f;

	rendering::g_menu.set_animating_close(!menu_target_open && var->gui.menu_render_visible);

	if (!var->gui.menu_render_visible)
	{
		var->gui.menu_was_closed = true;
		
		if (var->gui.watermark_spawn_alpha > 0.001f)  watermark->render();
		if (var->gui.active_hotkeys_spawn_alpha > 0.001f) { keybinds->update(); keybinds->render(); }
		if (var->gui.spotify_spawn_alpha > 0.001f)    { musicplayer->update(); musicplayer->render(); }
		if (var->gui.spectator_list_spawn_alpha > 0.001f) spectatorlist->render();
		return;
	}

	
		float scale_progress = var->gui.menu_open_alpha;
		scale_progress = scale_progress * scale_progress;  
		float menu_scale = 0.85f + (scale_progress * 0.15f);  

		gui->initialize();
		gui->set_next_window_size(SCALE(elements->window.size));
		

		gui->begin(elements->window.name, nullptr, window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_bring_to_front_on_focus | window_flags_no_focus_on_appearing | window_flags_no_background | window_flags_no_decoration);
		{
			c_window* window = gui->get_window();
			{
				const ImVec2 display = ImGui::GetIO().DisplaySize;
				ImVec2 pos = window->Pos;
				const ImVec2 size = window->Size;
				static ImVec2 last_display{};
				if (last_display.x > 1.0f && last_display.y > 1.0f &&
					(fabsf(last_display.x - display.x) > 0.5f || fabsf(last_display.y - display.y) > 0.5f))
				{
					pos.x *= display.x / last_display.x;
					pos.y *= display.y / last_display.y;
				}
				last_display = display;
				const float max_x = (std::max)(0.0f, display.x - size.x);
				const float max_y = (std::max)(0.0f, display.y - size.y);
				pos.x = std::clamp(pos.x, 0.0f, max_x);
				pos.y = std::clamp(pos.y, 0.0f, max_y);
				if (pos.x != window->Pos.x || pos.y != window->Pos.y)
					ImGui::SetWindowPos(pos);
			}
			c_vec2 pos = window->Pos;
			c_vec2 size = window->Size;
			
			ImVec2 menu_center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
			ImVec2 scaled_pos = ImVec2(
				menu_center.x + (pos.x - menu_center.x) * menu_scale,
				menu_center.y + (pos.y - menu_center.y) * menu_scale
			);
			ImVec2 scaled_size = ImVec2(size.x * menu_scale, size.y * menu_scale);
			
			var->gui.menu_scaled_pos = scaled_pos;
			var->gui.menu_scaled_size = scaled_size;
			var->gui.menu_scale = menu_scale;
			
			c_draw_list* draw_list = gui->background_drawlist();

			float shadow_blur = SCALE(12.0f);
			for (int s = 0; s < 6; s++)
			{
				float p  = (float)s / 6.0f;
				float a  = (1.0f - p) * 0.25f * var->gui.menu_open_alpha;
				float bl = shadow_blur * p;
				draw_list->AddRectFilled(
					ImVec2(scaled_pos.x - bl, scaled_pos.y - bl),
					ImVec2(scaled_pos.x + scaled_size.x + bl, scaled_pos.y + scaled_size.y + bl),
					IM_COL32(0, 0, 0, (int)(a * 255)),
					SCALE(elements->window.rounding) + bl
				);
			}

			ImVec4 bg_color = menu_theme::window_bg( var->gui.menu_open_alpha );

			draw_list->AddRectFilled(
				scaled_pos,
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
				draw->get_clr(bg_color),
				SCALE(elements->window.rounding)
			);

			ImVec4 bar_color = menu_theme::window_bar( var->gui.menu_open_alpha );
			float bar_height = SCALE(30.0f);  

			
			draw_list->AddRectFilled(
				scaled_pos,
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + bar_height),
				draw->get_clr(bar_color),
				SCALE(elements->window.rounding),
				ImDrawFlags_RoundCornersTop
			);

			
			float shadow_height = SCALE(12.0f);
			draw_list->AddRectFilledMultiColor(
				ImVec2(scaled_pos.x, scaled_pos.y + bar_height),
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + bar_height + shadow_height),
				IM_COL32(0, 0, 0, (int)(100 * var->gui.menu_open_alpha)),
				IM_COL32(0, 0, 0, (int)(100 * var->gui.menu_open_alpha)),
				IM_COL32(0, 0, 0, 0),
				IM_COL32(0, 0, 0, 0)
			);

			
			ImVec4 bar_outline_color = ImVec4(0.28f, 0.28f, 0.31f, 0.74f * var->gui.menu_open_alpha);
			draw_list->AddRect(
				scaled_pos,
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + bar_height),
				draw->get_clr(bar_outline_color),
				SCALE(elements->window.rounding),
				ImDrawFlags_RoundCornersTop,
				SCALE(1.0f)
			);

			
			draw_list->AddRectFilled(
				ImVec2(scaled_pos.x, scaled_pos.y + scaled_size.y - bar_height),
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
				draw->get_clr(bar_color),
				SCALE(elements->window.rounding),
				ImDrawFlags_RoundCornersBottom
			);

			
			draw_list->AddRectFilledMultiColor(
				ImVec2(scaled_pos.x, scaled_pos.y + scaled_size.y - bar_height - shadow_height),
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y - bar_height),
				IM_COL32(0, 0, 0, 0),
				IM_COL32(0, 0, 0, 0),
				IM_COL32(0, 0, 0, (int)(100 * var->gui.menu_open_alpha)),
				IM_COL32(0, 0, 0, (int)(100 * var->gui.menu_open_alpha))
			);

			
			draw_list->AddRect(
				ImVec2(scaled_pos.x, scaled_pos.y + scaled_size.y - bar_height),
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
				draw->get_clr(bar_outline_color),
				SCALE(elements->window.rounding),
				ImDrawFlags_RoundCornersBottom,
				SCALE(1.0f)
			);

			
			ImVec4 menu_outline_color = menu_theme::border( var->gui.menu_open_alpha );
			draw_list->AddRect(
				scaled_pos,
				ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
				draw->get_clr(menu_outline_color),
				SCALE(elements->window.rounding),
				0,
				SCALE(1.5f)
			);
			
			
			{
				
				const char* subtab_names[5][8] = {
					{"Players", "World", "View", "Effects", nullptr, nullptr, nullptr, nullptr},
					{"Legit", "Rage", "Anti-Aim", nullptr, nullptr, nullptr, nullptr, nullptr},
					{"Movement", "Misc", "Removals", nullptr, nullptr, nullptr, nullptr, nullptr},
					{"Skins", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
					{"Config", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}
				};
				
				int subtab_count = 0;
				for (int i = 0; i < 8; i++)
				{
					if (subtab_names[var->gui.active_tab][i] != nullptr)
						subtab_count++;
				}
				
				if (subtab_count > 0)
				{
					ImFont* subtab_font = font->get(main_font_data, menu_typography::k_control);
					if (subtab_font)
					{
						float subtab_spacing = SCALE(28.0f);
						float total_width = 0.0f;
						
						
						for (int i = 0; i < subtab_count; i++)
						{
							ImVec2 text_size = subtab_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, subtab_names[var->gui.active_tab][i]);
							total_width += text_size.x;
							if (i < subtab_count - 1) total_width += subtab_spacing;
						}
						
						
						float start_x = scaled_pos.x + (scaled_size.x - total_width) * 0.5f;
						float baseline_y = scaled_pos.y + scaled_size.y - bar_height * 0.5f;
						
						float current_x = start_x;
						
						for (int i = 0; i < subtab_count; i++)
						{
							const char* subtab_text = subtab_names[var->gui.active_tab][i];
							ImVec2 text_size = subtab_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, subtab_text);
							
							ImVec2 text_pos = ImVec2(current_x, baseline_y - text_size.y * 0.5f);
							
							
							ImRect hitbox(text_pos, ImVec2(text_pos.x + text_size.x, text_pos.y + text_size.y + SCALE(8.0f)));
							bool is_hovered = hitbox.Contains(ImGui::GetMousePos());
							bool is_active = (var->gui.active_subtab == i);
							
							if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && var->gui.active_subtab != i)
							{
								menu_interaction::close_popups_for_navigation();
								var->gui.active_subtab = i;
								var->gui.tab_content_anim = 0.0f;
							}
							
							
							ImVec4 text_color;
							if (is_active)
								text_color = ImVec4(179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, var->gui.menu_open_alpha);
							else if (is_hovered)
								text_color = ImVec4(0.85f, 0.85f, 0.88f, var->gui.menu_open_alpha);
							else
								text_color = ImVec4(0.65f, 0.65f, 0.7f, var->gui.menu_open_alpha);
							
							
							draw_list->AddText(subtab_font, menu_typography::k_control, text_pos, draw->get_clr(text_color), subtab_text);
							
							
							if (is_active)
							{
								float line_y = text_pos.y + text_size.y + SCALE(4.0f);
								float line_thickness = SCALE(2.0f);
								
								ImVec2 line_start = ImVec2(text_pos.x, line_y);
								ImVec2 line_end = ImVec2(text_pos.x + text_size.x, line_y);
								
								
								ImVec4 line_color = ImVec4(179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, var->gui.menu_open_alpha);
								draw_list->AddLine(line_start, line_end, draw->get_clr(line_color), line_thickness);
							}
							
							current_x += text_size.x + subtab_spacing;
						}
					}
				}
			}

			
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, var->gui.menu_open_alpha);

			
			float tab_icon_size = menu_typography::k_icon;
			float tab_spacing = SCALE(10.0f);     
			
			int tab_count = 5;
			
			
			float tab_start_x = scaled_pos.x + SCALE(12.0f);
			
			
			
			
			float icon_baseline_y = scaled_pos.y + bar_height * 0.5f;
			
			
			const char* tab_icons[] = {
				ICON_V2_VISUALS,    
				ICON_V2_AIM,        
				ICON_V2_EXPLOITS,   
				ICON_V2_SELF,       
				ICON_V2_SETTINGS    
			};
			
			ImFont* icon_font = font->get(main_font_data, tab_icon_size);
			
			
			struct tab_anim_state_t
			{
				float pop_scale = 1.0f;  
				float glow_time = 0.0f;  
			};
			static tab_anim_state_t tab_anims[5] = {};
			
			if (icon_font)
			{
				for (int i = 0; i < tab_count; i++)
				{
					float tab_x = tab_start_x + i * (tab_icon_size + tab_spacing);
					
					bool is_active = (var->gui.active_tab == i);
					
					
					float hitbox_size = SCALE(24.0f);
					float hitbox_x = tab_x - (hitbox_size - tab_icon_size) * 0.5f;
					float hitbox_y = icon_baseline_y - hitbox_size * 0.5f;
					
					bool is_hovered = ImGui::IsMouseHoveringRect(
						ImVec2(hitbox_x, hitbox_y), 
						ImVec2(hitbox_x + hitbox_size, hitbox_y + hitbox_size)
					);
					
					
					if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && var->gui.active_tab != i)
					{
						menu_interaction::close_popups_for_navigation();
						var->gui.active_tab = i;
						var->gui.active_subtab = 0;
						var->gui.tab_content_anim = 0.0f;
					}
					
					
					tab_anim_state_t* anim = &tab_anims[i];
					float target_scale = is_active ? 1.0f : (is_hovered ? 1.2f : 1.1f);  
					gui->easing(anim->pop_scale, target_scale, 12.0f, dynamic_easing);
					
					if (is_active)
					{
						anim->glow_time += ImGui::GetIO().DeltaTime;
					}
					
					
					if (!is_active)
					{
						
						float scaled_icon_size = tab_icon_size * anim->pop_scale;
						ImVec2 icon_size = icon_font->CalcTextSizeA(scaled_icon_size, FLT_MAX, 0.0f, tab_icons[i]);
						
						
						
						ImVec2 icon_pos = ImVec2(
							tab_x,
							icon_baseline_y - (icon_size.y * 0.5f)  
						);
						
						
						ImVec4 icon_color = ImVec4(
							0.75f, 0.75f, 0.8f, 
							var->gui.menu_open_alpha * (is_hovered ? 0.95f : 0.75f)
						);
						
						draw_list->AddText(icon_font, scaled_icon_size, icon_pos, draw->get_clr(icon_color), tab_icons[i]);
					}
					else
					{
						ImVec2 icon_size = icon_font->CalcTextSizeA(tab_icon_size, FLT_MAX, 0.0f, tab_icons[i]);
						ImVec2 icon_pos = ImVec2(
							tab_x,
							icon_baseline_y - (icon_size.y * 0.5f)
						);

						const ImVec4 glow_color = ImVec4(105.0f / 255.0f, 85.0f / 255.0f, 135.0f / 255.0f, 0.22f * var->gui.menu_open_alpha);
						const float glow_radius = SCALE(2.0f);
						static constexpr float k_glow_dirs[4][2] = {
							{ 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f }
						};
						for ( const auto& d : k_glow_dirs )
						{
							draw_list->AddText( icon_font, tab_icon_size,
								ImVec2( icon_pos.x + d[ 0 ] * glow_radius, icon_pos.y + d[ 1 ] * glow_radius ),
								draw->get_clr( glow_color ), tab_icons[ i ] );
						}

						const ImVec4 active_icon_color = ImVec4(
							130.0f / 255.0f, 110.0f / 255.0f, 170.0f / 255.0f,
							var->gui.menu_open_alpha
						);
						draw_list->AddText(icon_font, tab_icon_size, icon_pos, draw->get_clr(active_icon_color), tab_icons[i]);
					}
				}
			}
			
			{
				float button_size = SCALE(22.0f);  
				float button_gap = SCALE(8.0f);
				float searchbar_width = SCALE(180.0f);
				float searchbar_height = SCALE(22.0f);
				float searchbar_padding_right = SCALE(15.0f);
				
				
				
				ImVec2 searchbar_pos = ImVec2(
					scaled_pos.x + scaled_size.x - searchbar_width - searchbar_padding_right - button_size - button_gap,
					scaled_pos.y + (bar_height - searchbar_height) * 0.5f
				);
				
				float searchbar_rounding = SCALE(4.0f);
				
				ImVec2 searchbar_max = ImVec2(
					searchbar_pos.x + searchbar_width,
					searchbar_pos.y + searchbar_height
				);
				
				
				bool is_hovered = ImGui::IsMouseHoveringRect(searchbar_pos, searchbar_max);
				
				if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					var->gui.searchbar_active = true;
				}
				else if (!is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					var->gui.searchbar_active = false;
				}
				
				
				float target_glow = var->gui.searchbar_active ? 1.0f : 0.0f;
				gui->easing(var->gui.searchbar_glow_alpha, target_glow, 10.0f, dynamic_easing);
				
				
				
				
				float bg_brightness = var->gui.searchbar_active ? 30.0f : 25.0f;
				ImVec4 searchbar_bg = ImVec4(
					bg_brightness / 255.0f, 
					bg_brightness / 255.0f, 
					(bg_brightness + 3.0f) / 255.0f, 
					0.9f * var->gui.menu_open_alpha
				);
				draw_list->AddRectFilled(
					searchbar_pos,
					searchbar_max,
					draw->get_clr(searchbar_bg),
					searchbar_rounding
				);
				
				
				ImVec4 searchbar_border;
				if (var->gui.searchbar_active)
				{
					searchbar_border = ImVec4(
						179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, 
						0.4f * var->gui.menu_open_alpha
					);
				}
				else
				{
					searchbar_border = ImVec4(50.0f / 255.0f, 50.0f / 255.0f, 55.0f / 255.0f, 0.6f * var->gui.menu_open_alpha);
				}
				
				draw_list->AddRect(
					searchbar_pos,
					searchbar_max,
					draw->get_clr(searchbar_border),
					searchbar_rounding,
					0,
					SCALE(1.0f)
				);
				
				
				ImFont* search_icon_font = font->get(main_font_data, menu_typography::k_control);
				if (search_icon_font)
				{
					const char* search_icon = ICON_V2_SEARCH;
					ImVec2 icon_size = search_icon_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, search_icon);
					ImVec2 icon_pos = ImVec2(
						searchbar_pos.x + SCALE(8.0f),
						searchbar_pos.y + (searchbar_height - icon_size.y) * 0.5f
					);
					
					
					ImVec4 icon_color;
					if (var->gui.searchbar_active)
					{
						icon_color = ImVec4(179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, var->gui.menu_open_alpha * 0.9f);
					}
					else
					{
						icon_color = ImVec4(0.5f, 0.5f, 0.55f, var->gui.menu_open_alpha * 0.7f);
					}
					
					draw_list->AddText(search_icon_font, menu_typography::k_control, icon_pos, draw->get_clr(icon_color), search_icon);
				}
				
				
				ImFont* search_text_font = font->get(main_font_data, menu_typography::k_control);
				if (search_text_font)
				{
					float text_x = searchbar_pos.x + SCALE(26.0f);  
					float text_y = searchbar_pos.y + (searchbar_height * 0.5f);
					
					
					if (var->gui.searchbar_active)
					{
						ImGuiIO& io = ImGui::GetIO();
						
						
						if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && strlen(var->gui.search_buffer) > 0)
						{
							var->gui.search_buffer[strlen(var->gui.search_buffer) - 1] = '\0';
						}
						
						
						if (ImGui::IsKeyPressed(ImGuiKey_Escape))
						{
							var->gui.searchbar_active = false;
						}
						
						
						if (io.InputQueueCharacters.Size > 0)
						{
							for (int n = 0; n < io.InputQueueCharacters.Size; n++)
							{
								unsigned int c = (unsigned int)io.InputQueueCharacters[n];
								if (c >= 32 && c < 127)  
								{
									size_t len = strlen(var->gui.search_buffer);
									if (len < sizeof(var->gui.search_buffer) - 1)
									{
										var->gui.search_buffer[len] = (char)c;
										var->gui.search_buffer[len + 1] = '\0';
									}
								}
							}
						}
					}
					
					
					if (strlen(var->gui.search_buffer) > 0)
					{
						
						ImVec2 text_size = search_text_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, var->gui.search_buffer);
						ImVec2 text_pos = ImVec2(text_x, text_y - text_size.y * 0.5f);
						ImVec4 text_color = ImVec4(0.95f, 0.95f, 0.98f, var->gui.menu_open_alpha);
						draw_list->AddText(search_text_font, menu_typography::k_control, text_pos, draw->get_clr(text_color), var->gui.search_buffer);
						
						
						if (var->gui.searchbar_active)
						{
							float cursor_blink = (sinf(ImGui::GetTime() * 6.0f) + 1.0f) * 0.5f;
							ImVec2 cursor_pos = ImVec2(text_pos.x + text_size.x + SCALE(2.0f), text_pos.y);
							ImVec4 cursor_color = ImVec4(179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, cursor_blink * var->gui.menu_open_alpha);
							
							draw_list->AddLine(
								cursor_pos,
								ImVec2(cursor_pos.x, cursor_pos.y + text_size.y),
								draw->get_clr(cursor_color),
								SCALE(1.5f)
							);
						}
					}
					else
					{
						
						const char* placeholder = "Search feature...";
						ImVec2 text_size = search_text_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, placeholder);
						ImVec2 text_pos = ImVec2(text_x, text_y - text_size.y * 0.5f);
						ImVec4 text_color = ImVec4(0.45f, 0.45f, 0.5f, var->gui.menu_open_alpha * 0.6f);
						draw_list->AddText(search_text_font, menu_typography::k_control, text_pos, draw->get_clr(text_color), placeholder);
					}
				}
			}
			
			
			{
				float content_padding = SCALE(12.0f);
				float content_x = scaled_pos.x + content_padding;
				float content_y = scaled_pos.y + bar_height + content_padding;
				float content_width = scaled_size.x - (content_padding * 2.0f);
				float content_height = scaled_size.y - bar_height - bar_height - (content_padding * 2.0f);

				const bool players_preview_tab = (var->gui.active_tab == 0 && var->gui.active_subtab == 0);
				const float preview_gap = SCALE(12.0f);
				const float group_bar_h = players_preview_tab ? SCALE(28.0f) : 0.0f;
				float inner_padding = SCALE(10.0f);
				const float preview_w = players_preview_tab
					? (std::max)(SCALE(300.0f), content_width * 0.48f)
					: 0.0f;
				const float preview_h = players_preview_tab
					? (std::max)(SCALE(280.0f), content_height - inner_padding * 2.0f - group_bar_h - SCALE(6.0f))
					: 0.0f;

				ImVec4 child_bg = menu_theme::panel_bg( var->gui.menu_open_alpha );
				ImVec4 child_border = menu_theme::border( var->gui.menu_open_alpha );
				float child_rounding = SCALE( menu_theme::k_window_rounding );

				ImVec2 child_pos = ImVec2(content_x, content_y);
				ImVec2 child_max = ImVec2(content_x + content_width, content_y + content_height);

				draw_list->AddRectFilled(child_pos, child_max, draw->get_clr(child_bg), child_rounding);
				draw_list->AddRectFilledMultiColor(
					child_pos,
					ImVec2(child_max.x, child_pos.y + SCALE(48.0f)),
					draw->get_clr(menu_theme::accent_soft(0.10f * var->gui.menu_open_alpha)),
					draw->get_clr(menu_theme::accent_soft(0.05f * var->gui.menu_open_alpha)),
					IM_COL32(0, 0, 0, 0),
					IM_COL32(0, 0, 0, 0));
				draw_list->AddRect(
					ImVec2(child_pos.x + 0.5f, child_pos.y + 0.5f),
					ImVec2(child_max.x - 0.5f, child_max.y - 0.5f),
					draw->get_clr(child_border),
					child_rounding,
					0,
					SCALE(1.0f));
				draw_list->AddRect(
					ImVec2(child_pos.x + 1.0f, child_pos.y + 1.0f),
					ImVec2(child_max.x - 1.0f, child_max.y - 1.0f),
					draw->get_clr(ImVec4(0.30f, 0.30f, 0.33f, 0.72f * var->gui.menu_open_alpha)),
					child_rounding,
					0,
					SCALE(1.0f));

				ImVec2 content_inner_pos = ImVec2(child_pos.x + inner_padding, child_pos.y + inner_padding);
				float left_width = content_width - inner_padding * 2.0f;
				if (players_preview_tab)
					left_width = (std::max)(SCALE(190.0f), left_width - preview_w - preview_gap);
				ImVec2 content_inner_size = ImVec2(left_width, content_height - inner_padding * 2.0f);

				ImGui::SetCursorPos(ImVec2(
					content_inner_pos.x - scaled_pos.x,
					content_inner_pos.y - scaled_pos.y
				));

				ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
				ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SCALE(8.0f), SCALE(menu_theme::k_compact_spacing)));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SCALE(8.0f), SCALE(4.0f)));

				ImGuiWindowFlags content_flags = ImGuiWindowFlags_NoBackground;

				if (ImGui::BeginChild("##tab_content", content_inner_size, false, content_flags))
				{
					gui->easing(var->gui.tab_content_anim, 1.0f, menu_motion::k_content_fade, dynamic_easing);
					const float content_alpha = var->gui.tab_content_anim;
					const float slide = (1.0f - content_alpha) * SCALE(10.0f);
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + slide);
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_alpha * ImGui::GetStyle().Alpha);

					if (var->gui.active_tab == 0)
						menu_content::draw_visuals(var->gui.active_subtab);
					else if (var->gui.active_tab == 1)
						menu_content::draw_aim(var->gui.active_subtab);
					else if (var->gui.active_tab == 2)
						menu_content::draw_misc(var->gui.active_subtab);
					else if (var->gui.active_tab == 3)
						menu_content::draw_self(var->gui.active_subtab);
					else if (var->gui.active_tab == 4)
						menu_content::draw_settings(var->gui.active_subtab);

					ImGui::PopStyleVar();
				}
				ImGui::EndChild();

				ImGui::PopStyleVar(3);
				ImGui::PopStyleColor();

				if (players_preview_tab || var->gui.esp_preview_alpha > 0.01f)
				{
					const bool overlay_preview = var->gui.esp_preview_group >= 0 && var->gui.esp_preview_group <= 2;
					ImVec2 preview_size;
					ImVec2 preview_pos;
					const float group_bar_height = SCALE(28.0f);
					if (players_preview_tab)
					{
						preview_size = ImVec2(preview_w, preview_h);
						preview_pos = ImVec2(
							child_max.x - inner_padding - preview_w,
							child_max.y - inner_padding - preview_h);
					}
					else
					{
						preview_pos = var->gui.esp_preview_pos;
						preview_size = var->gui.esp_preview_size;
					}

					if (players_preview_tab && var->gui.side_panel_open && overlay_preview)
					{
						const float gap = SCALE(12.0f);
						var->gui.esp_feature_panel_pos = ImVec2(scaled_pos.x + scaled_size.x + gap, scaled_pos.y);
						var->gui.esp_feature_panel_size = ImVec2(SCALE(152.0f), SCALE(246.0f));
					}

					const float preview_a = var->gui.esp_preview_alpha;
					const bool preview_interactive = players_preview_tab && preview_a > 0.95f;
					ImGuiWindowFlags preview_flags =
						ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
						ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
						ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
					ImGuiWindowFlags overlay_flags = preview_flags | ImGuiWindowFlags_NoBringToFrontOnFocus;
					if (!preview_interactive)
						overlay_flags |= ImGuiWindowFlags_NoInputs;

					ImGui::SetNextWindowPos(preview_pos);
					ImGui::SetNextWindowSize(preview_size);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, preview_a);
					ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
					if (ImGui::Begin("##esp_preview_overlay", nullptr, overlay_flags))
					{
						widgets->render_esp_preview(preview_pos, preview_size);
					}
					ImGui::End();
					ImGui::PopStyleColor();
					ImGui::PopStyleVar(2);

					if (preview_size.x > 1.0f)
					{
						ImVec2 group_pos(preview_pos.x, preview_pos.y - group_bar_height - SCALE(6.0f));
						ImGui::SetNextWindowPos(group_pos);
						ImGui::SetNextWindowSize(ImVec2(preview_size.x, group_bar_height));
						ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, preview_a);
						ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
						if (ImGui::Begin("##esp_group_bar", nullptr, preview_flags))
						{
							ImDrawList* gdl = ImGui::GetWindowDrawList();
							const float group_round = SCALE(menu_theme::k_window_rounding);
							ImVec2 group_max(group_pos.x + preview_size.x, group_pos.y + group_bar_height);
							gdl->AddRectFilled(group_pos, group_max, draw->get_clr(menu_theme::panel_bg(preview_a)), group_round);
							if (players_preview_tab)
							{
								const float selector_w = SCALE(160.0f);
								ImGui::SetCursorPos(ImVec2((preview_size.x - selector_w) * 0.5f, (group_bar_height - SCALE(22.0f)) * 0.5f));
								const int previous_group = var->gui.esp_preview_group;
								draw_preview_group_selector(&var->gui.esp_preview_group);
								if (previous_group != var->gui.esp_preview_group)
								{
									var->gui.dragging_button_index = -1;
									var->gui.pending_esp_drop_feat = -1;
									var->gui.esp_preview_dragging_feat = -1;
									var->gui.esp_context_menu_open = -1;
									var->gui.esp_context_menu_feat = -1;
									var->gui.esp_drag_moved = false;
									menu_interaction::close_popups_for_navigation();
								}
								var->gui.esp_preview_side = std::clamp(var->gui.esp_preview_group, 0, 2);
							}
						}
						ImGui::End();
						ImGui::PopStyleColor();
						ImGui::PopStyleVar(2);
					}
				}
			}
			
			
			if (var->gui.active_tab == 0 && var->gui.active_subtab == 0 && var->gui.esp_preview_group >= 0 && var->gui.esp_preview_group <= 2)
			{
				float button_size = SCALE(22.0f);
				float button_gap = SCALE(8.0f);
				float searchbar_width = SCALE(180.0f);
				float searchbar_height = SCALE(22.0f);
				float searchbar_padding_right = SCALE(15.0f);
				
				
				ImVec2 searchbar_pos = ImVec2(
					scaled_pos.x + scaled_size.x - searchbar_width - searchbar_padding_right - button_size - button_gap,
					scaled_pos.y + (bar_height - searchbar_height) * 0.5f
				);
				
				
				ImVec2 button_pos = ImVec2(
					searchbar_pos.x + searchbar_width + button_gap,
					searchbar_pos.y
				);
				
				ImVec2 button_max = ImVec2(
					button_pos.x + button_size,
					button_pos.y + button_size
				);
				
				
				bool button_hovered = ImGui::IsMouseHoveringRect(button_pos, button_max);
				
				if (button_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					var->gui.side_panel_open = !var->gui.side_panel_open;
				}
				
				
				float target_rotation = var->gui.side_panel_open ? 180.0f : 0.0f;
				float delta = ImGui::GetIO().DeltaTime * 12.0f;
				if (var->gui.side_panel_toggle_rotation < target_rotation)
				{
					var->gui.side_panel_toggle_rotation += delta * (target_rotation - var->gui.side_panel_toggle_rotation);
					if (var->gui.side_panel_toggle_rotation > target_rotation)
						var->gui.side_panel_toggle_rotation = target_rotation;
				}
				else if (var->gui.side_panel_toggle_rotation > target_rotation)
				{
					var->gui.side_panel_toggle_rotation -= delta * (var->gui.side_panel_toggle_rotation - target_rotation);
					if (var->gui.side_panel_toggle_rotation < target_rotation)
						var->gui.side_panel_toggle_rotation = target_rotation;
				}
				
				
				ImVec4 button_bg;
				if (button_hovered)
					button_bg = ImVec4(40.0f / 255.0f, 40.0f / 255.0f, 45.0f / 255.0f, 0.9f * var->gui.menu_open_alpha);
				else
					button_bg = ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.8f * var->gui.menu_open_alpha);
				
				draw_list->AddRectFilled(button_pos, button_max, draw->get_clr(button_bg), SCALE(4.0f));
				
				
				ImVec4 button_border = ImVec4(50.0f / 255.0f, 50.0f / 255.0f, 55.0f / 255.0f, 0.6f * var->gui.menu_open_alpha);
				draw_list->AddRect(button_pos, button_max, draw->get_clr(button_border), SCALE(4.0f), 0, SCALE(1.0f));
				
				
				{
					ImVec2 icon_center = ImVec2(button_pos.x + button_size * 0.5f, button_pos.y + button_size * 0.5f);
					
					
					float angle_rad = var->gui.side_panel_toggle_rotation * (3.14159f / 180.0f);
					
					
					float chevron_size = SCALE(6.0f);  
					float chevron_height = SCALE(3.5f);  
					
					
					ImVec2 left_point = ImVec2(-chevron_size, -chevron_height);
					ImVec2 center_point = ImVec2(0, 0);
					ImVec2 right_point = ImVec2(chevron_size, -chevron_height);
					
					
					float cos_a = cosf(angle_rad);
					float sin_a = sinf(angle_rad);
					
					auto rotate = [&](ImVec2 p) -> ImVec2 {
						return ImVec2(
							icon_center.x + (p.x * cos_a - p.y * sin_a),
							icon_center.y + (p.x * sin_a + p.y * cos_a)
						);
					};
					
					ImVec2 p1 = rotate(left_point);
					ImVec2 p2 = rotate(center_point);
					ImVec2 p3 = rotate(right_point);
					
					
					ImVec4 chevron_color = ImVec4(0.7f, 0.7f, 0.75f, var->gui.menu_open_alpha);
					ImU32 col = draw->get_clr(chevron_color);
					
					draw_list->AddLine(p1, p2, col, SCALE(1.5f));
					draw_list->AddLine(p2, p3, col, SCALE(1.5f));
				}
			}

			ImGui::PopStyleVar();  
		}
		gui->end();

	if ( var->gui.search_navigation_found && !var->gui.search_navigation_pending )
	{
		var->gui.search_navigation_found = false;
		var->gui.search_navigate_to.clear( );
	}

	{
		const bool has_query = std::strlen( var->gui.search_buffer ) > 0;
		const bool show_results = has_query && var->gui.searchbar_active && var->gui.menu_open_alpha > 0.01f;
		gui->easing( var->gui.search_results_alpha, show_results ? 1.0f : 0.0f, menu_motion::k_content_fade, dynamic_easing );
		if ( var->gui.search_results_alpha > 0.01f )
		{
			std::vector< const menu_search_entry* > ranked;
			ranked.reserve( std::size( k_search_entries ) );
			for ( const auto& entry : k_search_entries )
			{
				if ( contains_insensitive( entry.label, var->gui.search_buffer ) )
					ranked.push_back( &entry );
			}

			std::sort( ranked.begin( ), ranked.end( ), [ & ]( const menu_search_entry* a, const menu_search_entry* b )
			{
				const int ra = search_match_rank( a->label, var->gui.search_buffer );
				const int rb = search_match_rank( b->label, var->gui.search_buffer );
				if ( ra != rb )
					return ra < rb;
				const auto la = std::strlen( a->label );
				const auto lb = std::strlen( b->label );
				if ( la != lb )
					return la < lb;
				return std::strcmp( a->label, b->label ) < 0;
			} );

			constexpr int k_search_max_results = 16;
			std::array< const menu_search_entry*, k_search_max_results > matches{};
			const int match_count = ( std::min )( static_cast< int >( ranked.size( ) ), k_search_max_results );
			for ( int i = 0; i < match_count; ++i )
				matches[ i ] = ranked[ static_cast< std::size_t >( i ) ];

			const float button_size = SCALE( 22.0f );
			const float button_gap = SCALE( 8.0f );
			const float searchbar_width = SCALE( 180.0f );
			const float searchbar_height = SCALE( 22.0f );
			const float searchbar_padding_right = SCALE( 15.0f );
			const ImVec2 scaled_pos = var->gui.menu_scaled_pos;
			const ImVec2 scaled_size = var->gui.menu_scaled_size;
			const ImVec2 searchbar_pos(
				scaled_pos.x + scaled_size.x - searchbar_width - searchbar_padding_right - button_size - button_gap,
				scaled_pos.y + ( SCALE( 30.0f ) - searchbar_height ) * 0.5f );
			const ImVec2 dropdown_min( searchbar_pos.x, searchbar_pos.y + searchbar_height + SCALE( 6.0f ) );
			const float row_h = SCALE( 24.0f );
			const int visible_rows = ( std::max )( 1, match_count );
			const float dropdown_h = row_h * visible_rows + SCALE( 10.0f );
			const ImVec2 dropdown_max( dropdown_min.x + SCALE( 220.0f ), dropdown_min.y + dropdown_h );
			ImDrawList* fg = ImGui::GetForegroundDrawList( );

			fg->AddRectFilled( dropdown_min, dropdown_max, draw->get_clr( menu_theme::panel_bg( var->gui.search_results_alpha ) ), SCALE( menu_theme::k_panel_rounding ) );
			fg->AddRect( dropdown_min, dropdown_max, draw->get_clr( menu_theme::border( var->gui.search_results_alpha ) ), SCALE( menu_theme::k_panel_rounding ), 0, SCALE( 1.0f ) );

			ImFont* text_font = font->get( main_font_data, menu_typography::k_control );
			if ( text_font )
			{
				const ImVec2 mouse = ImGui::GetMousePos( );
				var->gui.search_hovered_result = -1;

				if ( match_count == 0 )
				{
					fg->AddText( text_font, menu_typography::k_control, ImVec2( dropdown_min.x + SCALE( 10.0f ), dropdown_min.y + SCALE( 7.0f ) ),
						draw->get_clr( menu_theme::text_muted( var->gui.search_results_alpha ) ), "No matching features" );
				}

				for ( int i = 0; i < match_count; ++i )
				{
					const ImRect row(
						ImVec2( dropdown_min.x + SCALE( 5.0f ), dropdown_min.y + SCALE( 5.0f ) + row_h * i ),
						ImVec2( dropdown_max.x - SCALE( 5.0f ), dropdown_min.y + SCALE( 5.0f ) + row_h * ( i + 1 ) ) );
					const bool hovered = row.Contains( mouse );
					if ( hovered )
						var->gui.search_hovered_result = i;

					if ( hovered )
					{
						fg->AddRectFilled( row.Min, row.Max, draw->get_clr( menu_theme::accent_soft( 0.22f * var->gui.search_results_alpha ) ), SCALE( menu_theme::k_control_rounding ) );
						if ( ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
							navigate_to_search_result( *matches[ i ] );
					}

					fg->AddText(
						text_font,
						menu_typography::k_control,
						ImVec2( row.Min.x + SCALE( 8.0f ), row.Min.y + SCALE( 5.0f ) ),
						draw->get_clr( hovered ? menu_theme::text( var->gui.search_results_alpha ) : menu_theme::text_muted( var->gui.search_results_alpha ) ),
						matches[ i ]->label );
				}

				if ( match_count > 0 && var->gui.searchbar_active && ImGui::IsKeyPressed( ImGuiKey_Enter ) )
					navigate_to_search_result( *matches[ 0 ] );
			}
		}
	}

	if (var->gui.esp_feature_panel_alpha > 0.01f)
	{
		ImDrawList* draw_list = ImGui::GetForegroundDrawList();
		const ImVec2 scaled_menu_pos = var->gui.menu_scaled_pos;
		const ImVec2 scaled_menu_size = var->gui.menu_scaled_size;
		const float panel_a = var->gui.esp_feature_panel_alpha;
		const bool panel_live =
			var->gui.active_tab == 0 &&
			var->gui.active_subtab == 0 &&
			var->gui.side_panel_open &&
			var->gui.esp_preview_group >= 0 && var->gui.esp_preview_group <= 2;

		if (panel_live)
		{
			const float gap = SCALE(12.0f);
			var->gui.esp_feature_panel_pos = ImVec2(scaled_menu_pos.x + scaled_menu_size.x + gap, scaled_menu_pos.y);
			var->gui.esp_feature_panel_size = ImVec2(SCALE(152.0f), SCALE(246.0f));
		}

		const ImVec2 features_pos = var->gui.esp_feature_panel_pos;
		const ImVec2 features_size = var->gui.esp_feature_panel_size;
		if (features_size.x > 1.0f && features_size.y > 1.0f)
		{
			const float blur = SCALE(5.0f);
			draw_list->AddRectFilled(
				ImVec2(features_pos.x - blur, features_pos.y - blur),
				ImVec2(features_pos.x + features_size.x + blur, features_pos.y + features_size.y + blur),
				IM_COL32(0, 0, 0, (int)(0.14f * 255 * panel_a)),
				SCALE(8.0f));

			const float panel_round = SCALE(menu_theme::k_window_rounding);
			draw_list->AddRectFilled(
				features_pos,
				ImVec2(features_pos.x + features_size.x, features_pos.y + features_size.y),
				draw->get_clr(menu_theme::panel_bg(panel_a)),
				panel_round);
			draw_list->AddRect(
				features_pos,
				ImVec2(features_pos.x + features_size.x, features_pos.y + features_size.y),
				draw->get_clr(menu_theme::border(panel_a)),
				panel_round, 0, SCALE(1.5f));

			const char* button_labels[] = { "Box", "Skeleton", "Healthbar", "Ammobar", "Name", "Weapon", "Flags" };
			widgets->render_draggable_buttons(features_pos, features_size, button_labels, 7);
		}
	}
	else
	{
		var->gui.esp_feature_panel_pos = ImVec2(0, 0);
		var->gui.esp_feature_panel_size = ImVec2(0, 0);
	}
	
	if (var->gui.watermark_spawn_alpha > 0.001f)
		watermark->render();

	if (var->gui.active_hotkeys_spawn_alpha > 0.001f)
	{
		keybinds->update();
		keybinds->render();
	}

	if (var->gui.spectator_list_spawn_alpha > 0.001f)
		spectatorlist->render();

	widgets->render_bind_popup();

	widgets->render_esp_feature_context();

	if (welcome_bar)
		welcome_bar->render();

	if (var->gui.spotify_spawn_alpha > 0.001f)
	{
		musicplayer->update();
		musicplayer->render();
	}
}

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include "gui.menu_content.inl"
#include "gui.context.inl"

void rendering::register_hud_layout( )
{
	const auto reg_pos = []( ImVec2& pos, std::string_view name )
	{
		const auto reg_axis = [ & ]( float& axis, std::string_view suffix )
		{
			const auto full = std::string( name ) + std::string( suffix );
			config::detail::register_field( {
				.key = config::detail::make_key( "hud layout", full ),
				.type = config::field_type::float_val,
				.ptr = &axis,
				.count = 1 } );
		};

		reg_axis( pos.x, " x" );
		reg_axis( pos.y, " y" );
	};

	reg_pos( keybinds->layout_position( ), "keybind list" );
	reg_pos( spectatorlist->layout_position( ), "spectator list" );
	reg_pos( musicplayer->layout_position( ), "music player" );
	reg_pos( watermark->layout_position( ), "watermark" );
}
