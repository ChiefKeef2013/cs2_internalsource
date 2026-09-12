#pragma once
#include <string>
#include <vector>
#include <imgui.h>
#include "../headers/flags.h"

class c_variables
{
public:

	struct
	{
		float dpi = 1.f;
		int stored_dpi = 100;
		bool dpi_changed = true;
		bool menu_open = true;
		bool menu_render_visible = true;
		int active_tab = 0;
		float tab_transition_alpha = 1.0f;
		float menu_open_alpha = 1.0f;
		bool menu_was_closed = false;
		
		ImVec2 menu_scaled_pos = ImVec2(0, 0);
		ImVec2 menu_scaled_size = ImVec2(0, 0);
		float menu_scale = 1.0f;
		
		bool show_playerlist = true;
		bool show_spotify = false;
		bool show_active_hotkeys = false;
		bool show_watermark = true;
		bool show_spectator_list = true;
		
		bool playerlist_spawned = false;
		bool spotify_spawned = false;
		bool active_hotkeys_spawned = false;
		bool watermark_spawned = false;
		bool spectator_list_spawned = false;
		
		float playerlist_spawn_alpha = 0.0f;
		float spotify_spawn_alpha = 0.0f;
		float active_hotkeys_spawn_alpha = 0.0f;
		float watermark_spawn_alpha = 1.0f;
		float spectator_list_spawn_alpha = 1.0f;
		
		char search_buffer[256] = "";
		bool searchbar_active = false;
		float searchbar_glow_alpha = 0.0f;
		float search_results_alpha = 0.0f;
		int search_hovered_result = -1;
		std::string search_navigate_to{};
		bool search_navigation_pending = false;
		bool search_navigation_found = false;
		
		bool side_panel_open = true;
		float side_panel_toggle_rotation = 0.0f;
		
		int dragging_button_index = -1;
		int pending_esp_drop_feat = -1;
		ImVec2 pending_esp_drop_pos = ImVec2(0, 0);
		ImVec2 drag_offset = ImVec2(0, 0);
		std::vector<int> button_order = { 0, 1, 2, 3, 4, 5, 6 };
		std::vector<float> button_anim_y = { 0, 0, 0, 0, 0, 0, 0 };
		
		std::vector<int> active_esp_features;
		float esp_preview_health = 100.0f;
		float esp_preview_armor = 100.0f;
		bool esp_health_decreasing = true;
		
		int esp_healthbar_slot = 2;
		int esp_armorbar_slot = 1;
		int esp_name_slot = 0;
		int esp_weapon_slot = 1;
		int esp_flags_slot = 3;
		
		int esp_context_menu_open = -1;
		int esp_context_menu_feat = -1;
		float esp_context_menu_alpha = 0.0f;
		ImVec2 esp_context_menu_pos = ImVec2(0, 0);
		bool popup_blocks_input_prev = false;
		bool popup_blocks_input = false;
		bool dropdown_blocks_input_prev = false;
		bool dropdown_blocks_input = false;
		bool color_picker_open_prev = false;
		bool color_picker_open = false;
		bool bind_popup_visible_prev = false;
		bool bind_popup_visible = false;
		ImRect bind_popup_rect{};
		ImGuiID active_popup_id = 0;
		int popup_close_epoch = 0;
		
		int active_subtab = 0;
		float tab_content_anim = 1.0f;

		ImVec2 esp_preview_pos = ImVec2(0, 0);
		ImVec2 esp_preview_size = ImVec2(0, 0);
		bool esp_preview_visible = false;
		float esp_preview_alpha = 0.0f;
		float esp_feature_panel_alpha = 0.0f;
		int esp_preview_side = 0;
		int esp_preview_group = 0;
		ImVec2 esp_feature_panel_pos = ImVec2(0, 0);
		ImVec2 esp_feature_panel_size = ImVec2(0, 0);
		ImVec2 esp_drag_start_pos = ImVec2(0, 0);
		ImVec2 esp_preview_drag_offset = ImVec2(0, 0);
		bool esp_drag_moved = false;
		int esp_preview_dragging_feat = -1;
		bool overlay_blocks_welcome = false;
		ImVec2 overlay_block_min = ImVec2(0, 0);
		ImVec2 overlay_block_max = ImVec2(0, 0);
	} gui;

	gui_style style;

};

inline std::unique_ptr<c_variables> var = std::make_unique<c_variables>();
