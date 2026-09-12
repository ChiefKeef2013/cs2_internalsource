#include "../headers/functions.h"
#include "../headers/widgets.h"

namespace {

	struct slider_anim_t
	{
		float fill_width{ 0.0f };
		float hover_alpha{ 0.0f };
		float value_hover_alpha{ 0.0f };
		float value_active_alpha{ 0.0f };
	};

	const ImVec4 k_track_bg{ 0.13f, 0.13f, 0.15f, 1.0f };
	const ImVec4 k_track_fill{ 0.60f, 0.48f, 0.80f, 1.0f };
	const ImVec4 k_border{ 0.29f, 0.29f, 0.32f, 0.92f };
	const ImVec4 k_text{ 0.90f, 0.90f, 0.92f, 1.0f };
	const ImVec4 k_text_muted{ 0.72f, 0.72f, 0.77f, 1.0f };
	const ImVec4 k_value_bg{ 0.11f, 0.11f, 0.13f, 1.0f };
	const ImVec4 k_value_bg_hover{ 0.14f, 0.14f, 0.16f, 1.0f };
	const ImVec4 k_value_border{ 0.24f, 0.24f, 0.28f, 0.90f };
	const ImVec4 k_value_border_active{ 0.60f, 0.48f, 0.80f, 0.95f };

	struct slider_layout_t
	{
		ImGuiWindow* window{};
		ImFont* text_font{};
		ImGuiID id{};
		bool hide_label{};
		ImVec2 label_pos{};
		ImRect full_rect{};
		ImRect track_hit_rect{};
		ImRect track_rect{};
		ImRect value_rect{};
	};

	float measure_value_box_width(ImFont* font, std::initializer_list<const char*> samples, float min_width)
	{
		if (!font)
			return min_width;

		float max_text_width = 0.0f;
		for (const char* sample : samples)
		{
			if (!sample)
				continue;

			const float width = font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, sample).x;
			max_text_width = (std::max)(max_text_width, width);
		}

		return (std::max)(min_width, max_text_width + SCALE(16.0f));
	}

	bool build_layout(std::string_view name, float value_box_width, slider_layout_t& layout)
	{
		if (!gui || !draw || !font)
			return false;

		layout.window = gui->get_window();
		if (!layout.window)
			return false;

		layout.text_font = font->get(main_font_data, menu_typography::k_control);
		if (!layout.text_font)
			return false;

		layout.id = layout.window->GetID(name.data());
		layout.hide_label = name.size() >= 2 && name[0] == '#' && name[1] == '#';

		const float top_offset = SCALE(1.0f);
		const float row_height = SCALE(24.0f);
		const float track_height = SCALE(5.0f);
		const float value_height = SCALE(20.0f);
		const float gap = SCALE(10.0f);
		const float min_track_width = SCALE(70.0f);

		ImVec2 pos = layout.window->DC.CursorPos;
		pos.y += top_offset;

		const float max_total_width = (std::max)(gui->content_avail().x, min_track_width + value_box_width + gap);
		float label_width = 0.0f;
		if (!layout.hide_label)
		{
			const float measured = layout.text_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, name.data()).x;
			const float max_label_width = (std::max)(0.0f, max_total_width * 0.38f);
			label_width = (std::min)(measured + SCALE(4.0f), max_label_width);
		}

		const float required_width = label_width + value_box_width + gap * (layout.hide_label ? 1.0f : 2.0f) + min_track_width;
		if (required_width > max_total_width && !layout.hide_label)
			label_width = (std::max)(0.0f, max_total_width - value_box_width - min_track_width - gap * 2.0f);

		const float track_x = pos.x + label_width + (layout.hide_label ? 0.0f : gap);
		const float value_x = pos.x + max_total_width - value_box_width;
		const float track_width = (std::max)(min_track_width, value_x - track_x - gap);
		const float track_y = pos.y + (row_height - track_height) * 0.5f;
		const float value_y = pos.y + (row_height - value_height) * 0.5f;

		layout.full_rect = ImRect(pos, ImVec2(pos.x + max_total_width, pos.y + row_height + SCALE(2.0f)));
		layout.label_pos = ImVec2(pos.x, pos.y + (row_height - layout.text_font->FontSize) * 0.5f);
		layout.track_hit_rect = ImRect(
			ImVec2(track_x, pos.y),
			ImVec2(track_x + track_width, pos.y + row_height));
		layout.track_rect = ImRect(
			ImVec2(track_x, track_y),
			ImVec2(track_x + track_width, track_y + track_height));
		layout.value_rect = ImRect(
			ImVec2(value_x, value_y),
			ImVec2(value_x + value_box_width, value_y + value_height));
		return true;
	}

	void push_value_style()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SCALE(5.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(SCALE(6.0f), SCALE(2.0f)));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_NavHighlight, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Text, k_text);
		ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, IM_COL32(0, 0, 0, 0));
	}

	void pop_value_style()
	{
		ImGui::PopStyleColor(7);
		ImGui::PopStyleVar(3);
	}

	void draw_value_box_shell(const slider_layout_t& layout, const slider_anim_t& anim)
	{
		const float rounding = SCALE(5.0f);

		const ImVec4 bg(
			k_value_bg.x + (k_value_bg_hover.x - k_value_bg.x) * anim.value_hover_alpha,
			k_value_bg.y + (k_value_bg_hover.y - k_value_bg.y) * anim.value_hover_alpha,
			k_value_bg.z + (k_value_bg_hover.z - k_value_bg.z) * anim.value_hover_alpha,
			k_value_bg.w);
		draw->rect_filled(gui->window_drawlist(), layout.value_rect.Min, layout.value_rect.Max, draw->get_clr(bg), rounding);
		if (anim.value_active_alpha > 0.01f)
		{
			draw->rect_filled(
				gui->window_drawlist(),
				layout.value_rect.Min,
				layout.value_rect.Max,
				draw->get_clr(k_track_fill, 0.12f * anim.value_active_alpha),
				rounding);
		}
		const ImVec4 border(
			k_value_border.x + (k_value_border_active.x - k_value_border.x) * anim.value_active_alpha,
			k_value_border.y + (k_value_border_active.y - k_value_border.y) * anim.value_active_alpha,
			k_value_border.z + (k_value_border_active.z - k_value_border.z) * anim.value_active_alpha,
			k_value_border.w + (k_value_border_active.w - k_value_border.w) * anim.value_active_alpha);
		draw->rect(gui->window_drawlist(), layout.value_rect.Min, layout.value_rect.Max, draw->get_clr(border), rounding, 0, SCALE(1.0f));
	}

	void draw_slider_base(const slider_layout_t& layout, slider_anim_t& anim, std::string_view name)
	{
		const float rounding = SCALE(3.0f);

		if (anim.hover_alpha > 0.01f)
			draw->rect_filled(gui->window_drawlist(), layout.track_hit_rect.Min, layout.track_hit_rect.Max, draw->get_clr(k_track_fill, 0.04f * anim.hover_alpha), SCALE(5.0f));
		draw->rect_filled(gui->window_drawlist(), layout.track_rect.Min, layout.track_rect.Max, draw->get_clr(k_track_bg), rounding);

		if (anim.fill_width > SCALE(1.0f))
		{
			draw->rect_filled(
				gui->window_drawlist(),
				layout.track_rect.Min,
				ImVec2(layout.track_rect.Min.x + anim.fill_width, layout.track_rect.Max.y),
				draw->get_clr(k_track_fill),
				rounding);

			const float handle_radius = SCALE(5.0f);
			const ImVec2 center(
				layout.track_rect.Min.x + anim.fill_width,
				layout.track_rect.Min.y + layout.track_rect.GetHeight() * 0.5f);
			gui->window_drawlist()->AddCircleFilled(center, handle_radius, draw->get_clr(ImVec4(0.88f, 0.86f, 0.92f, 1.0f)), 24);
			gui->window_drawlist()->AddCircle(center, handle_radius, draw->get_clr(k_border), 24, SCALE(1.0f));
		}

		if (!layout.hide_label)
		{
			gui->window_drawlist()->PushClipRect(
				ImVec2(layout.label_pos.x, layout.full_rect.Min.y),
				ImVec2((std::max)(layout.label_pos.x, layout.track_rect.Min.x - SCALE(4.0f)), layout.full_rect.Max.y),
				true);
			draw->text(gui->window_drawlist(), layout.text_font, menu_typography::k_control, layout.label_pos, draw->get_clr(k_text_muted), name.data());
			gui->window_drawlist()->PopClipRect();
		}
	}

}

void c_widgets::slider_int(std::string_view name, int* value, int min, int max)
{
	if (!value || min >= max)
		return;

	ImFont* text_font = font->get(main_font_data, menu_typography::k_control);
	if (!text_font)
		return;

	char current_text[32]{};
	char min_text[32]{};
	char max_text[32]{};
	std::snprintf(current_text, sizeof(current_text), "%d", *value);
	std::snprintf(min_text, sizeof(min_text), "%d", min);
	std::snprintf(max_text, sizeof(max_text), "%d", max);

	slider_layout_t layout{};
	if (!build_layout(name, measure_value_box_width(text_font, { current_text, min_text, max_text }, SCALE(42.0f)), layout))
		return;

	slider_anim_t* anim = gui->anim_container<slider_anim_t>(layout.id);
	if (!anim)
		return;

	const float progress = static_cast<float>(*value - min) / static_cast<float>(max - min);
	const float clamped_progress = ImClamp(progress, 0.0f, 1.0f);
	const float target_fill = layout.track_rect.GetWidth() * clamped_progress;

	bool hovered = layout.track_hit_rect.Contains(gui->mouse_pos());
	bool held = false;
	gui->button_behavior(layout.track_hit_rect, layout.id, &hovered, &held);
	if (held)
	{
		const float t = ImClamp((gui->mouse_pos().x - layout.track_rect.Min.x) / layout.track_rect.GetWidth(), 0.0f, 1.0f);
		*value = min + static_cast<int>(t * static_cast<float>(max - min));
		*value = ImClamp(*value, min, max);
	}

	gui->easing(anim->fill_width, target_fill, menu_motion::k_control_active, dynamic_easing);
	gui->easing(anim->hover_alpha, hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, dynamic_easing);

	ImGui::PushID(name.data());
	const ImGuiID input_id = layout.window->GetID("##value");
	const bool input_hovered_predicted = layout.value_rect.Contains(gui->mouse_pos());
	const bool input_active_predicted = ImGui::GetActiveID() == input_id || (input_hovered_predicted && ImGui::IsMouseDown(ImGuiMouseButton_Left));
	gui->easing(anim->value_hover_alpha, input_hovered_predicted ? 1.0f : 0.0f, menu_motion::k_control_hover, dynamic_easing);
	gui->easing(anim->value_active_alpha, input_active_predicted ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing);

	draw_slider_base(layout, *anim, name);
	draw_value_box_shell(layout, *anim);

	const ImVec2 cursor_backup = layout.window->DC.CursorPos;
	ImGui::SetCursorScreenPos(layout.value_rect.Min);
	ImGui::SetNextItemWidth(layout.value_rect.GetWidth());
	push_value_style();
	gui->push_font(text_font);
	int input_value = *value;
	if (ImGui::InputInt("##value", &input_value, 0, 0, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_NoHorizontalScroll))
		*value = ImClamp(input_value, min, max);
	const bool input_hovered = ImGui::IsItemHovered();
	const bool input_active = ImGui::IsItemActive();
	gui->pop_font();
	pop_value_style();
	layout.window->DC.CursorPos = cursor_backup;
	ImGui::PopID();
	if (!input_active)
		gui->easing(anim->value_active_alpha, 0.0f, menu_motion::k_control_active, dynamic_easing);
	if (!input_hovered)
		gui->easing(anim->value_hover_alpha, 0.0f, menu_motion::k_control_hover, dynamic_easing);

	render_tooltip(name, hovered);
	gui->item_size(layout.full_rect);
	gui->item_add(layout.full_rect, layout.id);
	menu_search::focus_item_if_requested(name);
}

void c_widgets::slider_float(std::string_view name, float* value, float min, float max)
{
	if (!value || min >= max)
		return;

	ImFont* text_font = font->get(main_font_data, menu_typography::k_control);
	if (!text_font)
		return;

	char current_text[32]{};
	char min_text[32]{};
	char max_text[32]{};
	std::snprintf(current_text, sizeof(current_text), "%.2f", *value);
	std::snprintf(min_text, sizeof(min_text), "%.2f", min);
	std::snprintf(max_text, sizeof(max_text), "%.2f", max);

	slider_layout_t layout{};
	if (!build_layout(name, measure_value_box_width(text_font, { current_text, min_text, max_text }, SCALE(52.0f)), layout))
		return;

	slider_anim_t* anim = gui->anim_container<slider_anim_t>(layout.id);
	if (!anim)
		return;

	const float progress = (*value - min) / (max - min);
	const float clamped_progress = ImClamp(progress, 0.0f, 1.0f);
	const float target_fill = layout.track_rect.GetWidth() * clamped_progress;

	bool hovered = layout.track_hit_rect.Contains(gui->mouse_pos());
	bool held = false;
	gui->button_behavior(layout.track_hit_rect, layout.id, &hovered, &held);
	if (held)
	{
		const float t = ImClamp((gui->mouse_pos().x - layout.track_rect.Min.x) / layout.track_rect.GetWidth(), 0.0f, 1.0f);
		*value = ImClamp(min + t * (max - min), min, max);
	}

	gui->easing(anim->fill_width, target_fill, menu_motion::k_control_active, dynamic_easing);
	gui->easing(anim->hover_alpha, hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, dynamic_easing);

	ImGui::PushID(name.data());
	const ImGuiID input_id = layout.window->GetID("##value");
	const bool input_hovered_predicted = layout.value_rect.Contains(gui->mouse_pos());
	const bool input_active_predicted = ImGui::GetActiveID() == input_id || (input_hovered_predicted && ImGui::IsMouseDown(ImGuiMouseButton_Left));
	gui->easing(anim->value_hover_alpha, input_hovered_predicted ? 1.0f : 0.0f, menu_motion::k_control_hover, dynamic_easing);
	gui->easing(anim->value_active_alpha, input_active_predicted ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing);

	draw_slider_base(layout, *anim, name);
	draw_value_box_shell(layout, *anim);

	const ImVec2 cursor_backup = layout.window->DC.CursorPos;
	ImGui::SetCursorScreenPos(layout.value_rect.Min);
	ImGui::SetNextItemWidth(layout.value_rect.GetWidth());
	push_value_style();
	gui->push_font(text_font);
	float input_value = *value;
	if (ImGui::InputFloat("##value", &input_value, 0.0f, 0.0f, "%.2f", ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_NoHorizontalScroll))
		*value = ImClamp(input_value, min, max);
	const bool input_hovered = ImGui::IsItemHovered();
	const bool input_active = ImGui::IsItemActive();
	gui->pop_font();
	pop_value_style();
	layout.window->DC.CursorPos = cursor_backup;
	ImGui::PopID();
	if (!input_active)
		gui->easing(anim->value_active_alpha, 0.0f, menu_motion::k_control_active, dynamic_easing);
	if (!input_hovered)
		gui->easing(anim->value_hover_alpha, 0.0f, menu_motion::k_control_hover, dynamic_easing);

	render_tooltip(name, hovered);
	gui->item_size(layout.full_rect);
	gui->item_add(layout.full_rect, layout.id);
	menu_search::focus_item_if_requested(name);
}
