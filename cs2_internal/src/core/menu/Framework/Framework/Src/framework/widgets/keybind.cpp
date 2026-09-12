#include "../headers/functions.h"
#include "../headers/widgets.h"
#include "../data/IconsFontAwesome6.h"
#include <xdraw/xui/xui.hpp>
#include <Windows.h>
#include <cstdint>
#include <cstdio>

namespace {

	int bind_mode_index(xui::bind_mode mode)
	{
		switch (mode)
		{
		case xui::bind_mode::hold_on:
			return 1;
		case xui::bind_mode::hold_off:
			return 2;
		default:
			return 0;
		}
	}

	xui::bind_mode bind_mode_from_index(int index)
	{
		switch (index)
		{
		case 1:
			return xui::bind_mode::hold_on;
		case 2:
			return xui::bind_mode::hold_off;
		default:
			return xui::bind_mode::toggle;
		}
	}

	void push_popup_style()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SCALE(12.0f), SCALE(10.0f)));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SCALE(8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SCALE(8.0f), SCALE(menu_theme::k_compact_spacing)));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.17f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.28f, 0.31f, 0.90f));
	}

	void pop_popup_style()
	{
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}

	struct bind_popup_anim_t
	{
		bool open{ false };
		float alpha{ 0.0f };
		float offset{ 0.0f };
		ImVec2 anchor{};
		ImVec2 trigger_min{};
		ImVec2 trigger_max{};
		ImVec2 size{};
		int opened_frame{ -1 };
		int open_epoch{ -1 };
	};

	bool try_capture_bind_key( bool initial_state[256], bool ready, int& out_vk )
	{
		for ( int vk = 1; vk < 256; ++vk )
		{
			if ( vk == VK_ESCAPE ) continue;
			const bool down = ( GetAsyncKeyState( vk ) & 0x8000 ) != 0;

			if ( !ready )
			{
				initial_state[vk] = down;
				continue;
			}

			if ( initial_state[vk] )
			{
				if ( !down ) initial_state[vk] = false;
				continue;
			}

			if ( down )
			{
				out_vk = vk;
				return true;
			}
		}
		return false;
	}

	ImGuiID g_active_bind_popup_id{};
	xui::setting* g_active_bind_setting{};
	ImGuiID g_fading_bind_popup_id{};
	xui::setting* g_fading_bind_setting{};

	ImGuiID bind_popup_id( const xui::setting& setting )
	{
		const auto ptr = reinterpret_cast< std::uintptr_t >( &setting );
		return ImHashData( &ptr, sizeof( ptr ) );
	}
}

void c_widgets::keybind(std::string_view id, xui::setting& setting)
{
	if (!gui || !draw || !font)
		return;

	ImGuiWindow* window = gui->get_window();
	if (!window)
		return;

	ImFont* text_font = font->get(main_font_data, menu_typography::k_control);
	if (!text_font)
		return;

	struct anim_t
	{
		bool listening{ false };
		double listen_start_time{ 0.0 };
		bool initial_state[256]{};
		float pulse{ 0.0f };
	};

	ImGui::PushID(id.data());
	const ImGuiID widget_id = window->GetID("kb");
	anim_t* anim = gui->anim_container<anim_t>(widget_id);
	if (!anim)
	{
		ImGui::PopID();
		return;
	}

	char label[40]{};
	if (anim->listening)
		std::snprintf(label, sizeof(label), "...");
	else if (setting.bind.key == 0)
		std::snprintf(label, sizeof(label), "-");
	else
		std::snprintf(label, sizeof(label), "%s", xui::vk_name(setting.bind.key));

	ImGui::PushFont(text_font);
	const ImVec2 text_size = ImGui::CalcTextSize(label);
	ImGui::PopFont();

	const float pad_x = SCALE(8.0f);
	const float height = SCALE(22.0f);
	const float width = (std::max)(text_size.x + pad_x * 2.0f, SCALE(52.0f));
	const ImVec2 pos = window->DC.CursorPos;
	const ImRect rect(pos, pos + ImVec2(width, height));

	ImGui::InvisibleButton("##hit", ImVec2(width, height));
	const bool hovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		anim->listening = true;
		anim->listen_start_time = ImGui::GetTime();
		for ( int i = 0; i < 256; ++i ) anim->initial_state[i] = true;
	}

	if (anim->listening)
	{
		const double elapsed = ImGui::GetTime() - anim->listen_start_time;
		const bool ready = elapsed >= 0.5;

		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			setting.bind.key = 0;
			anim->listening = false;
		}
		else
		{
			int vk = 0;
			if (try_capture_bind_key(anim->initial_state, ready, vk))
			{
				setting.bind.key = vk;
				anim->listening = false;
			}
		}
	}

	gui->easing(anim->pulse, anim->listening ? 1.0f : (hovered ? 0.35f : 0.0f), menu_motion::k_control_hover, dynamic_easing);

	const ImVec4 bg{ 0.14f, 0.14f, 0.16f, 1.0f };
	const ImVec4 border{
		0.28f + anim->pulse * 0.14f,
		0.28f + anim->pulse * 0.10f,
		0.31f + anim->pulse * 0.12f,
		0.95f
	};
	const ImVec4 text = anim->listening
		? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
		: ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
	const float rounding = SCALE(6.0f);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(bg), rounding);
	dl->AddRect(rect.Min, rect.Max, ImGui::ColorConvertFloat4ToU32(border), rounding, 0, SCALE(1.0f));

	const ImVec2 text_pos(
		rect.Min.x + (width - text_size.x) * 0.5f,
		rect.Min.y + (height - text_size.y) * 0.5f);
	dl->AddText(text_font, text_font->FontSize, text_pos, ImGui::ColorConvertFloat4ToU32(text), label);

	ImGui::PopID();
}

void c_widgets::checkbox_bind(std::string_view name, xui::setting& setting)
{
	if (!gui || !draw || !font)
		return;

	const bool before = setting.value;
	checkbox(name, &setting.value);
	if (setting.value != before)
		setting.bind.active = setting.value;

	const ImRect trigger_rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
	const ImGuiID popup_id = bind_popup_id(setting);
	auto* popup_anim = gui->anim_container<bind_popup_anim_t>(popup_id);
	if (!popup_anim)
		return;

	if (popup_anim->open && g_active_bind_popup_id == popup_id)
	{
		g_active_bind_setting = &setting;
		popup_anim->trigger_min = trigger_rect.Min;
		popup_anim->trigger_max = trigger_rect.Max;
	}

	const bool hovered =
		gui->is_window_hovered(ImGuiHoveredFlags_None) &&
		trigger_rect.Contains(ImGui::GetMousePos());
	if (!hovered ||
		var->gui.dropdown_blocks_input ||
		var->gui.color_picker_open ||
		!gui->mouse_clicked(mouse_button_right))
	{
		return;
	}

	const bool closing_same = popup_anim->open && g_active_bind_popup_id == popup_id;
	if (g_active_bind_popup_id != 0 && g_active_bind_popup_id != popup_id)
	{
		if (auto* previous = gui->anim_container<bind_popup_anim_t>(g_active_bind_popup_id))
			previous->open = false;
		g_fading_bind_popup_id = g_active_bind_popup_id;
		g_fading_bind_setting = g_active_bind_setting;
	}

	popup_anim->anchor = trigger_rect.Max;
	popup_anim->trigger_min = trigger_rect.Min;
	popup_anim->trigger_max = trigger_rect.Max;
	popup_anim->open = !closing_same;
	if (closing_same)
		return;

	if (popup_anim->alpha < 0.01f)
	{
		popup_anim->alpha = 0.0f;
		popup_anim->offset = SCALE(8.0f);
	}
	popup_anim->opened_frame = ImGui::GetFrameCount();
	popup_anim->open_epoch = var->gui.popup_close_epoch;
	g_active_bind_popup_id = popup_id;
	g_active_bind_setting = &setting;
}

namespace {

	bool draw_bind_popup_window(ImGuiID popup_id, xui::setting* setting, bool is_active_slot)
	{
		if (!setting)
			return false;

		auto* popup_anim = gui->anim_container<bind_popup_anim_t>(popup_id);
		if (!popup_anim)
			return false;

		if (popup_anim->open_epoch >= 0 && popup_anim->open_epoch != var->gui.popup_close_epoch)
		{
			popup_anim->open = false;
			popup_anim->open_epoch = var->gui.popup_close_epoch;
		}

		const float popup_speed = popup_anim->open ? menu_motion::k_popup_open : menu_motion::k_popup_close;
		gui->easing(popup_anim->alpha, popup_anim->open ? 1.0f : 0.0f, popup_speed, dynamic_easing);
		gui->easing(popup_anim->offset, popup_anim->open ? 0.0f : SCALE(8.0f), popup_speed, dynamic_easing);

		if (!popup_anim->open && popup_anim->alpha <= 0.01f)
			return false;

		const ImRect anchor_rect(popup_anim->trigger_min, popup_anim->trigger_max);
		var->gui.popup_blocks_input = true;
		const ImVec2 base_popup_pos = menu_interaction::popup_position_near_rect(
			anchor_rect,
			ImVec2(SCALE(220.0f), popup_anim->size.y > 0.0f ? popup_anim->size.y : SCALE(132.0f)),
			SCALE(8.0f));
		ImGui::SetNextWindowPos(
			ImVec2(
				base_popup_pos.x,
				base_popup_pos.y + SCALE(8.0f) * (1.0f - popup_anim->alpha)),
			ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(SCALE(220.0f), 0.0f));
		ImGui::SetNextWindowBgAlpha(0.0f);
		push_popup_style();
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, popup_anim->alpha);

		const bool interactive = is_active_slot && popup_anim->open && popup_anim->alpha > 0.05f;
		ImGuiWindowFlags popup_flags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse;
		if (!interactive)
			popup_flags |= ImGuiWindowFlags_NoInputs;

		char window_name[64]{};
		std::snprintf(window_name, sizeof(window_name), "##bind_popup_window_%u", static_cast<unsigned>(popup_id));
		if (ImGui::Begin(window_name, nullptr, popup_flags))
		{
			const ImVec2 wp = ImGui::GetWindowPos();
			const ImVec2 ws = ImGui::GetWindowSize();
			popup_anim->size = ws;
			const ImRect popup_rect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y));
			if (is_active_slot)
			{
				var->gui.bind_popup_visible = true;
				var->gui.bind_popup_rect = popup_rect;
			}
			menu_interaction::mark_welcome_overlay(popup_rect);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const float rounding = SCALE(8.0f);
			dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), draw->get_clr(ImVec4(38.0f / 255.0f, 38.0f / 255.0f, 44.0f / 255.0f, 250.0f / 255.0f)), rounding);
			dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + SCALE(32.0f)), draw->get_clr(ImVec4(44.0f / 255.0f, 44.0f / 255.0f, 50.0f / 255.0f, 1.0f)), rounding, ImDrawFlags_RoundCornersTop);
			dl->AddRect(ImVec2(wp.x + 0.5f, wp.y + 0.5f), ImVec2(wp.x + ws.x - 0.5f, wp.y + ws.y - 0.5f), draw->get_clr(ImVec4(78.0f / 255.0f, 78.0f / 255.0f, 86.0f / 255.0f, 220.0f / 255.0f)), rounding, 0, SCALE(1.0f));
			ImGui::SetCursorPos(ImVec2(SCALE(12.0f), SCALE(9.0f)));
			ImFont* text_font = font->get(main_font_data, menu_typography::k_control);
			if (text_font)
				gui->push_font(text_font);
			ImGui::TextUnformatted("Bind");
			if (text_font)
				gui->pop_font();
			ImGui::SetCursorPosY(SCALE(38.0f) + popup_anim->offset * 0.25f);
			ImGui::SetNextItemWidth(SCALE(96.0f));
			widgets->keybind("key", *setting);
			int mode = bind_mode_index(setting->bind.mode);
			const char* modes[]{ "toggle", "hold", "hold off" };
			widgets->dropdown("mode", &mode, modes, 3);
			setting->bind.mode = bind_mode_from_index(mode);

			const ImRect keep_open(popup_anim->trigger_min, popup_anim->trigger_max);
			if (popup_anim->open &&
				menu_interaction::popup_outside_clicked(popup_rect, popup_anim->opened_frame) &&
				!keep_open.Contains(ImGui::GetMousePos()))
			{
				popup_anim->open = false;
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
		pop_popup_style();
		return true;
	}

}

void c_widgets::render_bind_popup()
{
	if (!gui || !draw || !font)
		return;

	if (g_fading_bind_popup_id != 0)
	{
		if (g_fading_bind_popup_id == g_active_bind_popup_id ||
			!draw_bind_popup_window(g_fading_bind_popup_id, g_fading_bind_setting, false))
		{
			g_fading_bind_popup_id = 0;
			g_fading_bind_setting = nullptr;
		}
	}

	if (g_active_bind_popup_id == 0 || !g_active_bind_setting)
		return;

	if (!draw_bind_popup_window(g_active_bind_popup_id, g_active_bind_setting, true))
	{
		g_active_bind_popup_id = 0;
		g_active_bind_setting = nullptr;
	}
}
