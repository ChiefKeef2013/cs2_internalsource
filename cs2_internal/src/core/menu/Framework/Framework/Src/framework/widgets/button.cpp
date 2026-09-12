#include "../headers/functions.h"
#include "../headers/widgets.h"

namespace {

	const ImVec4 k_button_bg{ 0.15f, 0.15f, 0.17f, 0.96f };
	const ImVec4 k_button_bg_hover{ 0.18f, 0.18f, 0.20f, 0.98f };
	const ImVec4 k_button_border{ 0.28f, 0.28f, 0.31f, 0.85f };
	const ImVec4 k_button_text{ 0.90f, 0.90f, 0.92f, 1.0f };
	const ImVec4 k_button_accent{ 0.50f, 0.46f, 0.58f, 1.0f };

	struct button_anim_t
	{
		float hover_alpha{ 0.0f };
		float active_alpha{ 0.0f };
	};

}

bool c_widgets::button(std::string_view name, float width)
{
	if (!gui || !draw || !font)
		return false;

	ImGuiWindow* window = gui->get_window();
	if (!window)
		return false;

	ImFont* text_font = font->get(main_font_data, menu_typography::k_button);
	if (!text_font)
		return false;

	ImGuiID id = window->GetID(name.data());
	button_anim_t* anim = gui->anim_container<button_anim_t>(id);
	if (!anim)
		return false;

	const ImVec2 text_size = text_font->CalcTextSizeA(menu_typography::k_button, FLT_MAX, 0.0f, name.data());
	const float height = SCALE(22.0f);
	const float top_offset = SCALE(4.0f);
	const float rounding = SCALE(6.0f);
	if (width <= 0.0f)
		width = (std::max)(SCALE(120.0f), gui->content_avail().x - SCALE(10.0f));

	ImVec2 pos = window->DC.CursorPos;
	pos.y += top_offset;

	ImRect rect(pos, ImVec2(pos.x + width, pos.y + height));
	bool hovered = rect.Contains(ImGui::GetMousePos());
	bool held = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool clicked = hovered && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(ImGuiHoveredFlags_None);

	gui->easing(anim->hover_alpha, hovered ? 1.0f : 0.0f, 18.0f, dynamic_easing);
	gui->easing(anim->active_alpha, held ? 1.0f : 0.0f, 16.0f, dynamic_easing);

	const ImVec4 bg = hovered ? k_button_bg_hover : k_button_bg;
	const float shadow_alpha = 0.08f + anim->hover_alpha * 0.06f;
	const float shadow_blur = SCALE(8.0f);
	gui->window_drawlist()->AddRectFilled(
		ImVec2(rect.Min.x - shadow_blur * 0.35f, rect.Min.y - shadow_blur * 0.25f),
		ImVec2(rect.Max.x + shadow_blur * 0.35f, rect.Max.y + shadow_blur * 0.35f),
		IM_COL32(0, 0, 0, static_cast<int>(shadow_alpha * 255.0f)),
		rounding + SCALE(1.0f));

	draw->rect_filled(gui->window_drawlist(), rect.Min, rect.Max, draw->get_clr(bg), rounding);
	draw->rect(gui->window_drawlist(), rect.Min, rect.Max, draw->get_clr(k_button_border), rounding, 0, SCALE(1.0f));

	if (anim->active_alpha > 0.01f)
	{
		draw->rect_filled(
			gui->window_drawlist(),
			rect.Min,
			rect.Max,
			draw->get_clr(k_button_accent, 0.14f * anim->active_alpha),
			rounding);
	}

	const ImVec2 text_pos(
		rect.Min.x + (width - text_size.x) * 0.5f,
		rect.Min.y + (height - text_size.y) * 0.5f);
	draw->text(gui->window_drawlist(), text_font, menu_typography::k_button, text_pos, draw->get_clr(k_button_text), name.data());

	render_tooltip(name, hovered);
	gui->item_size(rect);
	gui->item_add(rect, id);
	menu_search::focus_item_if_requested(name);
	return clicked;
}

bool c_widgets::icon_button(std::string_view id_text, const char* icon, float size)
{
	if (!gui || !draw || !font || !icon || !icon[0])
		return false;

	ImGuiWindow* window = gui->get_window();
	if (!window)
		return false;

	ImFont* icon_font = font->get(main_font_data, size - SCALE(4.0f));
	if (!icon_font)
		return false;

	ImGuiID id = window->GetID(id_text.data());
	button_anim_t* anim = gui->anim_container<button_anim_t>(id);
	if (!anim)
		return false;

	const float button_size = SCALE(size);
	const float rounding = SCALE(5.0f);
	ImVec2 pos = window->DC.CursorPos;
	ImRect rect(pos, ImVec2(pos.x + button_size, pos.y + button_size));

	bool hovered = rect.Contains(ImGui::GetMousePos());
	bool held = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool clicked = hovered && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(ImGuiHoveredFlags_None);

	gui->easing(anim->hover_alpha, hovered ? 1.0f : 0.0f, 18.0f, dynamic_easing);
	gui->easing(anim->active_alpha, held ? 1.0f : 0.0f, 16.0f, dynamic_easing);

	if ( anim->hover_alpha > 0.01f || anim->active_alpha > 0.01f )
	{
		draw->rect_filled(
			gui->window_drawlist(),
			rect.Min,
			rect.Max,
			draw->get_clr( ImVec4( 0.18f, 0.18f, 0.20f, 0.90f ), 0.65f * anim->hover_alpha + 0.85f * anim->active_alpha ),
			rounding);
		draw->rect(
			gui->window_drawlist(),
			rect.Min,
			rect.Max,
			draw->get_clr( ImVec4( 0.30f, 0.30f, 0.34f, 0.88f ), 0.55f * anim->hover_alpha + 0.75f * anim->active_alpha ),
			rounding,
			0,
			SCALE(1.0f));
	}

	const ImVec2 icon_size = icon_font->CalcTextSizeA(icon_font->FontSize, FLT_MAX, 0.0f, icon);
	const ImVec2 icon_pos(
		rect.Min.x + (button_size - icon_size.x) * 0.5f,
		rect.Min.y + (button_size - icon_size.y) * 0.5f);
	const ImVec4 icon_color = hovered
		? ImVec4(0.96f, 0.96f, 0.98f, 1.0f)
		: ImVec4(0.84f, 0.84f, 0.88f, 0.96f);
	draw->text(gui->window_drawlist(), icon_font, icon_font->FontSize, icon_pos, draw->get_clr(icon_color), icon);

	gui->item_size(rect);
	gui->item_add(rect, id);
	return clicked;
}
