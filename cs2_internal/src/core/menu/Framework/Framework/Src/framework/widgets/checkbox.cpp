#include "../headers/functions.h"
#include "../headers/widgets.h"

void c_widgets::checkbox(std::string_view name, bool* value)
{
    if (!value || !gui || !draw || !font)
        return;

    struct anim_t
    {
        float hover_alpha{ 0 };
        float active_alpha{ 0 };
        float slow_active_alpha{ 0 };
        float held_alpha{ 0 };
    };

    ImGuiWindow* window = gui->get_window();
    if (!window)
        return;

    ImGuiID id = window->GetID(name.data());

    ImFont* text_font = font->get(main_font_data, menu_typography::k_control);
    if (!text_font)
        return;

    ImVec2 text_size = text_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, name.data(), gui->text_end(name.data()));

    float checkbox_size = SCALE(16.0f);
    float spacing = SCALE(7.0f);
    float top_offset = 0.0f;
    float rounding = SCALE(5.0f);

    ImVec2 pos = window->DC.CursorPos;
    pos.y += top_offset;

    ImRect checkbox_rect(pos, pos + ImVec2(checkbox_size, checkbox_size));
    ImRect full_rect(pos, ImVec2(pos.x + checkbox_size + spacing + text_size.x + SCALE(6.0f), pos.y + checkbox_size));

    anim_t* anim = gui->anim_container<anim_t>(id);
    if (!anim)
        return;

    bool hovered = full_rect.Contains(ImGui::GetMousePos());
    bool clicked = hovered && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(ImGuiHoveredFlags_None);
    bool mouse_down = hovered && ImGui::IsMouseDown(0);

    if (clicked)
        *value = !*value;

    gui->easing(anim->hover_alpha, (hovered || *value) ? 1.0f : 0.0f, 20.f, static_easing);
    gui->easing(anim->active_alpha, *value ? 1.0f : 0.0f, 20.f, dynamic_easing);
    gui->easing(anim->slow_active_alpha, *value ? 1.0f : 0.0f, 10.f, dynamic_easing);
    gui->easing(anim->held_alpha, (hovered && mouse_down) ? 0.0f : 1.0f, 8.f, dynamic_easing);

    ImVec4 checkbox_bg = ImVec4(0.15f, 0.15f, 0.17f, 0.96f);
    ImVec4 checkbox_bg_dark = ImVec4(0.13f, 0.13f, 0.15f, 0.96f);
    ImVec4 checkbox_bg_held = ImVec4(0.18f, 0.18f, 0.20f, 0.98f);
    ImVec4 active_fill = ImVec4(100.0f / 255.0f, 80.0f / 255.0f, 130.0f / 255.0f, 1.0f);
    ImVec4 text_color = ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
    ImVec4 text_color_disabled = ImVec4(0.6f, 0.6f, 0.65f, 1.0f);

    float shadow_strength = 0.5f + (0.2f * anim->hover_alpha);
    float shadow_blur = SCALE(8.0f);
    float shadow_offset = SCALE(1.5f * anim->hover_alpha);

    {
        float shadow_alpha = 0.18f * shadow_strength;
        gui->window_drawlist()->AddRectFilled(
            ImVec2(checkbox_rect.Min.x - shadow_blur * 0.35f + shadow_offset, checkbox_rect.Min.y - shadow_blur * 0.35f + shadow_offset),
            ImVec2(checkbox_rect.Max.x + shadow_blur * 0.35f + shadow_offset, checkbox_rect.Max.y + shadow_blur * 0.35f + shadow_offset),
            IM_COL32(0, 0, 0, (int)(shadow_alpha * 255)),
            rounding
        );
    }

    draw->rect_filled(gui->window_drawlist(),
        ImVec2(checkbox_rect.Min.x + SCALE(1.0f), checkbox_rect.Min.y + SCALE(1.0f)),
        checkbox_rect.Max,
        draw->get_clr(checkbox_bg_dark), rounding);

    draw->rect_filled(gui->window_drawlist(), checkbox_rect.Min, checkbox_rect.Max,
        draw->get_clr(checkbox_bg), rounding);

    draw->rect(gui->window_drawlist(), checkbox_rect.Min, checkbox_rect.Max,
        draw->get_clr(ImVec4(0.28f, 0.28f, 0.31f, 0.88f)), rounding, 0, SCALE(1.0f));

    if (anim->held_alpha < 0.99f)
    {
        float held_scale = anim->held_alpha;
        ImVec2 held_center = ImVec2(
            checkbox_rect.Min.x + checkbox_size * 0.5f,
            checkbox_rect.Min.y + checkbox_size * 0.5f
        );
        ImVec2 held_min = ImVec2(
            held_center.x - (checkbox_size * 0.5f * held_scale),
            held_center.y - (checkbox_size * 0.5f * held_scale)
        );
        ImVec2 held_max = ImVec2(
            held_center.x + (checkbox_size * 0.5f * held_scale),
            held_center.y + (checkbox_size * 0.5f * held_scale)
        );

        draw->rect_filled(gui->window_drawlist(), held_min, held_max,
            draw->get_clr(checkbox_bg_held, 1.0f - anim->held_alpha), rounding);
    }

    if (anim->active_alpha > 0.01f)
    {
        float quint_ease;
        if (*value)
            quint_ease = anim->active_alpha * anim->active_alpha * anim->active_alpha * anim->active_alpha * anim->active_alpha;
        else
        {
            float inv = 1.0f - anim->active_alpha;
            quint_ease = 1.0f - (inv * inv * inv * inv * inv);
        }

        ImVec2 fill_center = ImVec2(
            checkbox_rect.Min.x + checkbox_size * 0.5f,
            checkbox_rect.Min.y + checkbox_size * 0.5f
        );

        ImVec2 fill_size = ImVec2(checkbox_size * quint_ease, checkbox_size * quint_ease);
        ImVec2 fill_min = ImVec2(
            fill_center.x - fill_size.x * 0.5f,
            fill_center.y - fill_size.y * 0.5f
        );
        ImVec2 fill_max = ImVec2(
            fill_center.x + fill_size.x * 0.5f,
            fill_center.y + fill_size.y * 0.5f
        );

        float dynamic_rounding = rounding * (5.0f * (1.0f - quint_ease) + 1.0f);

        draw->rect_filled(gui->window_drawlist(), fill_min, fill_max,
            draw->get_clr(active_fill, quint_ease), dynamic_rounding);
    }

    if (anim->slow_active_alpha > 0.01f)
    {
        float slow_quint;
        if (*value)
            slow_quint = anim->slow_active_alpha * anim->slow_active_alpha * anim->slow_active_alpha * anim->slow_active_alpha * anim->slow_active_alpha;
        else
        {
            float inv = 1.0f - anim->slow_active_alpha;
            slow_quint = 1.0f - (inv * inv * inv * inv * inv);
        }

        float checkmark_size_total = SCALE(10.0f) * (*value ?
            (anim->active_alpha * anim->active_alpha * anim->active_alpha * anim->active_alpha * anim->active_alpha) :
            (1.0f - ((1.0f - anim->active_alpha) * (1.0f - anim->active_alpha) * (1.0f - anim->active_alpha) * (1.0f - anim->active_alpha) * (1.0f - anim->active_alpha))));

        ImVec2 center = ImVec2(
            checkbox_rect.Min.x + checkbox_size * 0.5f,
            checkbox_rect.Min.y + checkbox_size * 0.5f
        );

        float check_width = checkmark_size_total * 0.85f;
        float check_height = checkmark_size_total * 0.70f;

        ImVec2 p1 = ImVec2(
            std::roundf(center.x - check_width * 0.35f),
            std::roundf(center.y - check_height * 0.05f)
        );
        ImVec2 p2 = ImVec2(
            std::roundf(center.x - check_width * 0.05f),
            std::roundf(center.y + check_height * 0.35f)
        );
        ImVec2 p3 = ImVec2(
            std::roundf(center.x + check_width * 0.45f),
            std::roundf(center.y - check_height * 0.50f)
        );

        float thickness = SCALE(2.0f);
        float outline_thickness = SCALE(4.0f);

        ImU32 outline_color = draw->get_clr(ImVec4(0.20f, 0.20f, 0.23f, 0.92f), slow_quint);
        ImU32 check_color = draw->get_clr(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), slow_quint);

        for (int i = 0; i < 3; i++)
        {
            gui->window_drawlist()->PathLineTo(p1);
            gui->window_drawlist()->PathLineTo(p2);
            gui->window_drawlist()->PathLineTo(p3);
            gui->window_drawlist()->PathStroke(outline_color, ImDrawFlags_None, outline_thickness);
        }

        gui->window_drawlist()->PathLineTo(p1);
        gui->window_drawlist()->PathLineTo(p2);
        gui->window_drawlist()->PathLineTo(p3);
        gui->window_drawlist()->PathStroke(check_color, ImDrawFlags_None, thickness);
    }

    float text_offset_x = spacing + std::roundf((spacing * 0.5f) * anim->hover_alpha);
    ImVec2 text_pos = ImVec2(
        checkbox_rect.Max.x + text_offset_x,
        checkbox_rect.Min.y + (checkbox_size - text_size.y) * 0.5f
    );

    float text_quint = *value ?
        (anim->active_alpha * anim->active_alpha * anim->active_alpha * anim->active_alpha * anim->active_alpha) :
        (1.0f - ((1.0f - anim->active_alpha) * (1.0f - anim->active_alpha) * (1.0f - anim->active_alpha) * (1.0f - anim->active_alpha) * (1.0f - anim->active_alpha)));

    if (*value && text_quint > 0.01f)
    {
        float outline_offset = SCALE(1.0f);
        ImVec4 outline_color = ImVec4(0.20f, 0.20f, 0.23f, 0.90f * text_quint);

        draw->text(gui->window_drawlist(), text_font, menu_typography::k_control,
            ImVec2(text_pos.x + outline_offset, text_pos.y + outline_offset),
            draw->get_clr(outline_color), name.data());
    }

    draw->text(gui->window_drawlist(), text_font, menu_typography::k_control, text_pos,
        draw->get_clr(text_color_disabled, 1.0f - text_quint), name.data());

    draw->text(gui->window_drawlist(), text_font, menu_typography::k_control, text_pos,
        draw->get_clr(text_color, text_quint), name.data());

    render_tooltip(name, hovered);
    gui->item_size(full_rect);
    gui->item_add(full_rect, id);
    menu_search::focus_item_if_requested(name);
}
