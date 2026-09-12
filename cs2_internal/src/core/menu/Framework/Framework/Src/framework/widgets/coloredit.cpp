#include "../headers/functions.h"
#include "../headers/widgets.h"
#include <cctype>
#include <cmath>

void render_color_rect_with_alpha_checkboard(ImDrawList* draw_list, ImVec2 p_min, ImVec2 p_max, ImU32 col, float grid_step, ImVec2 grid_off, float rounding, ImDrawFlags flags)
{
    if ((flags & ImDrawFlags_RoundCornersMask_) == 0)
        flags = ImDrawFlags_RoundCornersDefault_;
    
    if (((col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT) < 0xFF)
    {
        ImU32 col_bg1 = draw->get_clr(ImColor(27, 27, 32));
        ImU32 col_bg2 = draw->get_clr(ImColor(46, 46, 56));
        draw_list->AddRectFilled(p_min, p_max, col_bg1, rounding, flags);
        
        int yi = 0;
        for (float y = p_min.y + grid_off.y; y < p_max.y; y += grid_step, yi++)
        {
            float y1 = ImClamp(y, p_min.y, p_max.y), y2 = ImMin(y + grid_step, p_max.y);
            if (y2 <= y1)
                continue;
            
            for (float x = p_min.x + grid_off.x + (yi & 1) * grid_step; x < p_max.x; x += grid_step * 2.0f)
            {
                float x1 = ImClamp(x, p_min.x, p_max.x), x2 = ImMin(x + grid_step, p_max.x);
                if (x2 <= x1)
                    continue;
                
                ImDrawFlags cell_flags = ImDrawFlags_RoundCornersNone;
                if (y1 <= p_min.y) { if (x1 <= p_min.x) cell_flags |= ImDrawFlags_RoundCornersTopLeft; if (x2 >= p_max.x) cell_flags |= ImDrawFlags_RoundCornersTopRight; }
                if (y2 >= p_max.y) { if (x1 <= p_min.x) cell_flags |= ImDrawFlags_RoundCornersBottomLeft; if (x2 >= p_max.x) cell_flags |= ImDrawFlags_RoundCornersBottomRight; }
                
                cell_flags = (flags == ImDrawFlags_RoundCornersNone || cell_flags == ImDrawFlags_RoundCornersNone) ? ImDrawFlags_RoundCornersNone : (cell_flags & flags);
                draw_list->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), col_bg2, rounding, cell_flags);
            }
        }
    }
    else
    {
        draw_list->AddRectFilled(p_min, p_max, col, rounding, flags);
    }
}

namespace {

    std::uint8_t color_channel_to_u8(float value)
    {
        return static_cast<std::uint8_t>(std::lroundf(std::clamp(value, 0.0f, 1.0f) * 255.0f));
    }

    void format_hex_color(const float* color, char* out, std::size_t out_size)
    {
        std::snprintf(out, out_size, "%02X%02X%02X%02X",
            color_channel_to_u8(color[0]),
            color_channel_to_u8(color[1]),
            color_channel_to_u8(color[2]),
            color_channel_to_u8(color[3]));
    }

    bool parse_hex_color(std::string_view input, float* color)
    {
        while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front())))
            input.remove_prefix(1);
        while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back())))
            input.remove_suffix(1);
        if (!input.empty() && input.front() == '#')
            input.remove_prefix(1);

        if (input.size() != 6 && input.size() != 8)
            return false;

        auto hex_value = [](char c) -> int
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };

        std::uint8_t channels[4]{ 0, 0, 0, 255 };
        for (std::size_t i = 0; i < input.size() / 2; ++i)
        {
            const int hi = hex_value(input[i * 2]);
            const int lo = hex_value(input[i * 2 + 1]);
            if (hi < 0 || lo < 0)
                return false;
            channels[i] = static_cast<std::uint8_t>((hi << 4) | lo);
        }

        color[0] = channels[0] / 255.0f;
        color[1] = channels[1] / 255.0f;
        color[2] = channels[2] / 255.0f;
        if (input.size() == 8)
            color[3] = channels[3] / 255.0f;
        return true;
    }

}

void c_widgets::color_picker(std::string_view name, float* color)
{
    struct anim_t
    {
        bool opened{ false };
        float alpha{ 0.f };
        float h{ -1 }, s{ -1 }, v{ -1 };
        float saved_hue{ 0.0f };
        float grab[4]{ 0.f };
        bool update_hsv{ false };
        float box_hover{ 0.f };
        float box_active{ 0.0f };
        char hex_input_buf[32]{};
        bool hex_focused{ false };
        bool hex_dirty{ false };
        ImU32 last_color{ 0 };
        float copy_hover_alpha{ 0.0f };
        float copy_active_alpha{ 0.0f };
        float paste_hover_alpha{ 0.0f };
        float paste_active_alpha{ 0.0f };
    };
    
    ImGuiWindow* window = gui->get_window();
    ImGuiID id = window->GetID(name.data());
    
    
    const bool hide_label = name.size() >= 2 && name[0] == '#' && name[1] == '#';
    float box_width = SCALE(30.0f);
    float box_height = SCALE(16.0f);
    float spacing = SCALE(8.0f);
    float row_height = hide_label ? box_height : SCALE(24.0f);
    
    ImVec2 pos = window->DC.CursorPos;
    pos.y += hide_label ? 0.0f : SCALE(1.0f);
    
    
    ImFont* text_font = font->get(main_font_data, menu_typography::k_control);
    ImVec2 text_size = hide_label ? ImVec2(0, 0) : gui->text_size(text_font, name.data(), gui->text_end(name.data()));
    
    
    float row_width = hide_label ? box_width : (std::max)(gui->content_avail().x, text_size.x + spacing + box_width);
    const float box_x = hide_label ? pos.x : (std::min)(pos.x + text_size.x + spacing, pos.x + row_width - box_width);
    const float box_y = hide_label ? pos.y : pos.y + (row_height - box_height) * 0.5f;
    ImRect full_rect(pos, ImVec2(pos.x + row_width, pos.y + row_height));
    ImRect box_rect(
        hide_label ? pos : ImVec2(box_x, box_y),
        hide_label ? ImVec2(pos.x + box_width, pos.y + box_height) : ImVec2(box_x + box_width, box_y + box_height));
    
    anim_t* anim = gui->anim_container<anim_t>(id);
    
    bool value_changed = false;
    
    ImColor col_hues[7] = { ImColor(255, 0, 0), ImColor(255, 255, 0), ImColor(0, 255, 0), ImColor(0, 255, 255), ImColor(0, 0, 255), ImColor(255, 0, 255), ImColor(255, 0, 0) };
    
    bool is_hovered = box_rect.Contains(gui->mouse_pos());

    const bool clicked_swatch = is_hovered && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(ImGuiHoveredFlags_None);
    if (clicked_swatch && !var->gui.dropdown_blocks_input)
    {
        anim->opened = !anim->opened;
        if (anim->opened)
            anim->update_hsv = true;
    }

    const float popup_speed = anim->opened ? menu_motion::k_popup_open : menu_motion::k_popup_close;
    gui->easing(anim->alpha, anim->opened ? 1.f : 0.f, popup_speed, dynamic_easing);
    gui->easing(anim->box_hover, is_hovered ? 1.f : 0.f, menu_motion::k_control_hover, static_easing);
    gui->easing(anim->box_active, anim->opened ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing);
    if (anim->opened || anim->alpha > 0.01f)
        var->gui.color_picker_open = true;
    
    
    ImVec4 box_bg = ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.95f);
    ImVec4 box_bg_dark = ImVec4(24.0f / 255.0f, 24.0f / 255.0f, 28.0f / 255.0f, 0.98f);
    
    
    float shadow_strength = 0.5f + (0.15f * anim->box_hover);
    float shadow_blur = SCALE(10.0f);
    float shadow_offset = SCALE(1.0f * anim->box_hover);
    
    for (int s = 0; s < 3; s++)
    {
        float shadow_progress = (float)s / 3.0f;
        float shadow_alpha = (1.0f - shadow_progress) * 0.12f * shadow_strength;
        float blur_amount = shadow_blur * shadow_progress;
        
        gui->window_drawlist()->AddRectFilled(
            ImVec2(box_rect.Min.x - blur_amount + shadow_offset, box_rect.Min.y - blur_amount + shadow_offset),
            ImVec2(box_rect.Max.x + blur_amount + shadow_offset, box_rect.Max.y + blur_amount + shadow_offset),
            IM_COL32(0, 0, 0, (int)(shadow_alpha * 255)),
            SCALE(5.0f)  
        );
    }
    
    
    draw->rect_filled(gui->window_drawlist(), 
        ImVec2(box_rect.Min.x + SCALE(0.5f), box_rect.Min.y + SCALE(0.5f)), 
        ImVec2(box_rect.Max.x - SCALE(0.5f), box_rect.Max.y - SCALE(0.5f)), 
        draw->get_clr(box_bg_dark), SCALE(5.0f));
    
    
    draw->rect_filled(gui->window_drawlist(), box_rect.Min, box_rect.Max, 
        draw->get_clr(box_bg), SCALE(5.0f));
    
    
    draw->rect(gui->window_drawlist(), ImVec2(box_rect.Min.x + SCALE(0.5f), box_rect.Min.y + SCALE(0.5f)), ImVec2(box_rect.Max.x - SCALE(0.5f), box_rect.Max.y - SCALE(0.5f)), 
        draw->get_clr(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)), SCALE(5.0f), 0, SCALE(1.0f));
    
    
    render_color_rect_with_alpha_checkboard(gui->window_drawlist(), box_rect.Min, box_rect.Max, 
        draw->get_clr({ 0.f, 0.f, 0.f, 0.f }), box_rect.GetWidth() / 4, ImVec2(0, 0), SCALE(5.0f), 0);
    
    
    draw->rect_filled(gui->window_drawlist(), box_rect.Min, box_rect.Max, 
        draw->get_clr({ color[0], color[1], color[2], color[3] }), SCALE(5.0f));
    
    
    if (!hide_label)
    {
        ImVec2 text_pos = ImVec2(
            pos.x,
            pos.y + (ImMax(box_height, text_size.y) - text_size.y) * 0.5f
        );
        
        ImVec4 text_color = ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
        draw->text(gui->window_drawlist(), text_font, menu_typography::k_control, text_pos,
            draw->get_clr(text_color), name.data());
    }
    
    const ImU32 packed_color = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
    if ((anim->h < 0.f || anim->s < 0.f || anim->v < 0.f) || anim->update_hsv || packed_color != anim->last_color)
    {
        gui->rgb_to_hsv(color[0], color[1], color[2], anim->h, anim->s, anim->v);
        if (anim->s <= 0.0f)
            anim->h = anim->saved_hue;
        else
            anim->saved_hue = anim->h;
        anim->update_hsv = false;
        anim->last_color = packed_color;
    }
    
    if (anim->alpha >= 0.01f)
    {
        const float popup_padding = SCALE(10.0f);
        const float popup_width = SCALE(270.0f);
        const float popup_rounding = SCALE(8.0f);
        const float popup_height = SCALE(360.0f);
        const ImVec2 popup_base = menu_interaction::popup_position_near_rect(
            box_rect,
            ImVec2( popup_width, popup_height ),
            SCALE(8.0f));
        const float popup_x = popup_base.x;
        const float popup_y = popup_base.y + SCALE(8.0f) * ( 1.0f - anim->alpha );
        const std::string popup_name = "##coloredit_window_" + std::to_string(static_cast<unsigned long long>(id));
        var->gui.popup_blocks_input = true;

        gui->push_var(style_var_alpha, anim->alpha);
        gui->push_var(style_var_item_spacing, SCALE(ImVec2(8, 8)));
        gui->push_var(style_var_window_padding, ImVec2(popup_padding, popup_padding));
        gui->push_var(style_var_window_rounding, popup_rounding);
        gui->push_var(style_var_popup_border_size, SCALE(0));
        
        
        gui->push_color(style_col_popup_bg, draw->get_clr(ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.95f)));
        gui->push_color(style_col_border, draw->get_clr(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)));
        
        gui->set_next_window_size(ImVec2(popup_width, 0.0f));
        gui->set_next_window_pos(ImVec2(popup_x, popup_y), ImGuiCond_Always);
        
        window_flags popup_flags =
            window_flags_always_use_window_padding |
            window_flags_no_saved_settings |
            window_flags_always_auto_resize |
            window_flags_no_scrollbar |
            window_flags_no_scroll_with_mouse |
            window_flags_no_collapse |
            window_flags_no_resize |
            window_flags_no_title_bar;
        if (!anim->opened && anim->alpha <= 0.05f)
            popup_flags |= window_flags_no_inputs;

        gui->begin(popup_name, nullptr, popup_flags);
        {
            const ImVec2 window_min = gui->window_pos();
            const ImVec2 window_max = gui->window_pos() + ImVec2(gui->window_width(), gui->window_height());
            const float window_rounding = popup_rounding;
            draw->rect_filled(gui->window_drawlist(), window_min, window_max,
                draw->get_clr(ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.95f)), window_rounding);
            draw->rect(gui->window_drawlist(), ImVec2(window_min.x + SCALE(0.5f), window_min.y + SCALE(0.5f)), ImVec2(window_max.x - SCALE(0.5f), window_max.y - SCALE(0.5f)),
                draw->get_clr(ImVec4(0.29f, 0.29f, 0.32f, 0.92f)), window_rounding, 0, SCALE(1.0f));

            float sv_size = popup_width - popup_padding * 2.0f;
            float sv_grab_size = SCALE(12.0f);
            float bar_height = SCALE(20.0f);
            float bar_width = SCALE(3.0f);
            float bar_padding = SCALE(2.0f);
            float rounding = SCALE(6.0f);
            
            
            float hex_container_width = sv_size;

            gui->invisible_button("sv_rect", ImVec2(sv_size, sv_size));
            if (gui->is_item_active())
            {
                anim->s = ImSaturate((gui->mouse_pos().x - (GImGui->LastItemData.Rect.Min.x + sv_grab_size / 2)) / (GImGui->LastItemData.Rect.GetWidth() - sv_grab_size));
                anim->v = 1.f - ImSaturate((gui->mouse_pos().y - (GImGui->LastItemData.Rect.Min.y + sv_grab_size / 2)) / (GImGui->LastItemData.Rect.GetHeight() - sv_grab_size));
                value_changed = true;
            }
            
            gui->easing(anim->grab[0], sv_grab_size / 2 + anim->s * (GImGui->LastItemData.Rect.GetWidth() - sv_grab_size), menu_motion::k_control_scroll, dynamic_easing);
            gui->easing(anim->grab[1], sv_grab_size / 2 + (1.f - anim->v) * (GImGui->LastItemData.Rect.GetHeight() - sv_grab_size), menu_motion::k_control_scroll, dynamic_easing);
            
            float R, G, B;
            ColorConvertHSVtoRGB(anim->h, 1.f, 1.f, R, G, B);
            draw->rect_filled_multi_color(gui->window_drawlist(), GImGui->LastItemData.Rect.Min, GImGui->LastItemData.Rect.Max, draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), draw->get_clr({ R, G, B, 1.f }), draw->get_clr({ R, G, B, 1.f }), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), rounding);
            draw->rect_filled_multi_color(gui->window_drawlist(), GImGui->LastItemData.Rect.Min, GImGui->LastItemData.Rect.Max, draw->get_clr({ 0.f, 0.f, 0.f, 0.f }), draw->get_clr({ 0.f, 0.f, 0.f, 0.f }), draw->get_clr({ 0.f, 0.f, 0.f, 1.f }), draw->get_clr({ 0.f, 0.f, 0.f, 1.f }), rounding - 1);
            draw->circle(gui->window_drawlist(), GImGui->LastItemData.Rect.Min + ImVec2(anim->grab[0], anim->grab[1]), sv_grab_size / 2, draw->get_clr(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 30, SCALE(1));
            
            
            gui->invisible_button("hue_bar", ImVec2(sv_size, bar_height));
            if (gui->is_item_active())
            {
                anim->h = ImSaturate((gui->mouse_pos().x - (GImGui->LastItemData.Rect.Min.x + bar_width / 2)) / (GImGui->LastItemData.Rect.GetWidth() - bar_width));
                value_changed = true;
            }
            
            gui->easing(anim->grab[2], bar_width / 2 + anim->h * (GImGui->LastItemData.Rect.GetWidth() - bar_width), menu_motion::k_control_scroll, dynamic_easing);
            
            for (int i = 0; i < IM_ARRAYSIZE(col_hues) - 1; ++i)
                draw->rect_filled_multi_color(gui->window_drawlist(), GImGui->LastItemData.Rect.Min + ImVec2(roundf(i * (GImGui->LastItemData.Rect.GetWidth() / 6)), bar_padding), ImVec2(GImGui->LastItemData.Rect.Min.x + roundf((i + 1) * (GImGui->LastItemData.Rect.GetWidth() / 6)), GImGui->LastItemData.Rect.Max.y - bar_padding), draw->get_clr(col_hues[i]), draw->get_clr(col_hues[i + 1]), draw->get_clr(col_hues[i + 1]), draw->get_clr(col_hues[i]), rounding, (i == 0) ? draw_flags_round_corners_left : (i == 5) ? draw_flags_round_corners_right : draw_flags_round_corners_none);
            
            draw->rect_filled(gui->window_drawlist(), GImGui->LastItemData.Rect.Min + ImVec2(anim->grab[2] - bar_width / 2, 0), GImGui->LastItemData.Rect.GetBL() + ImVec2(anim->grab[2] + bar_width / 2, 0), draw->get_clr(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), SCALE(2));
            
            
            gui->invisible_button("alpha_bar", ImVec2(sv_size, bar_height));
            if (gui->is_item_active())
                color[3] = ImSaturate((gui->mouse_pos().x - (GImGui->LastItemData.Rect.Min.x + bar_width / 2)) / (GImGui->LastItemData.Rect.GetWidth() - bar_width));
            
            gui->easing(anim->grab[3], bar_width / 2 + color[3] * (GImGui->LastItemData.Rect.GetWidth() - bar_width), menu_motion::k_control_scroll, dynamic_easing);
            
            draw->rect_filled_multi_color(gui->window_drawlist(), GImGui->LastItemData.Rect.Min + ImVec2(0, bar_padding), GImGui->LastItemData.Rect.Max - ImVec2(0, bar_padding), draw->get_clr(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), draw->get_clr({ R, G, B, 1.f }), draw->get_clr({ R, G, B, 1.f }), draw->get_clr(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), rounding);
            draw->rect_filled(gui->window_drawlist(), GImGui->LastItemData.Rect.Min + ImVec2(anim->grab[3] - bar_width / 2, 0), GImGui->LastItemData.Rect.GetBL() + ImVec2(anim->grab[3] + bar_width / 2, 0), draw->get_clr(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), SCALE(2));
            
            
            gui->dummy(SCALE(ImVec2(0, 6)));

            char hex_buffer[16]{};
            format_hex_color(color, hex_buffer, sizeof(hex_buffer));

            float container_width = hex_container_width;
            float button_width = SCALE(52.0f);
            float button_height = SCALE(26.0f);
            float button_gap = SCALE(6.0f);
            float swatch_w = SCALE(28.0f);
            float input_width = (std::max)(SCALE(90.0f), container_width - swatch_w - (button_width * 2) - (button_gap * 3));

            if (!anim->hex_focused || !anim->hex_dirty)
                strcpy_s(anim->hex_input_buf, hex_buffer);

            ImVec2 row_pos = ImGui::GetCursorScreenPos();
            ImRect swatch_rect(row_pos, ImVec2(row_pos.x + swatch_w, row_pos.y + button_height));
            ImVec2 input_cursor_pos = ImVec2(swatch_rect.Max.x + button_gap, row_pos.y);
            ImRect input_rect(input_cursor_pos, ImVec2(input_cursor_pos.x + input_width, input_cursor_pos.y + button_height));
            bool input_hovered = input_rect.Contains(ImGui::GetMousePos());

            render_color_rect_with_alpha_checkboard(gui->window_drawlist(), swatch_rect.Min, swatch_rect.Max,
                draw->get_clr({ 0.f, 0.f, 0.f, 0.f }), swatch_rect.GetWidth() / 4.0f, ImVec2(0, 0), SCALE(5.0f), 0);
            draw->rect_filled(gui->window_drawlist(), swatch_rect.Min, swatch_rect.Max,
                draw->get_clr({ color[0], color[1], color[2], color[3] }), SCALE(5.0f));
            draw->rect(gui->window_drawlist(), swatch_rect.Min, swatch_rect.Max,
                draw->get_clr(ImVec4(0.0f, 0.0f, 0.0f, 0.35f)), SCALE(5.0f), 0, SCALE(1.0f));

            ImVec4 input_bg = ImVec4(26.0f / 255.0f, 27.0f / 255.0f, 32.0f / 255.0f, 0.98f);
            draw->rect_filled(gui->window_drawlist(), input_rect.Min, input_rect.Max,
                draw->get_clr(input_bg), SCALE(5.0f));
            draw->rect(gui->window_drawlist(), input_rect.Min, input_rect.Max,
                draw->get_clr(anim->hex_focused
                    ? ImVec4(0.42f, 0.40f, 0.48f, 0.95f)
                    : input_hovered ? ImVec4(0.34f, 0.34f, 0.39f, 0.9f) : ImVec4(0.0f, 0.0f, 0.0f, 0.38f)),
                SCALE(5.0f), 0, SCALE(1.0f));

            ImGui::SetCursorScreenPos(input_cursor_pos);
            gui->push_var(style_var_frame_padding, SCALE(ImVec2(9, 5)));
            gui->push_var(style_var_frame_rounding, SCALE(5.0f));
            gui->push_var(style_var_frame_border_size, SCALE(0.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, draw->get_clr(ImVec4(0.88f, 0.88f, 0.88f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, draw->get_clr(ImVec4(0.42f, 0.40f, 0.48f, 0.35f)));

            ImGui::SetNextItemWidth(input_width);
            gui->push_font(text_font);
            if (ImGui::InputText("##hex_input", anim->hex_input_buf, sizeof(anim->hex_input_buf), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase))
            {
                anim->hex_dirty = true;
                if (parse_hex_color(anim->hex_input_buf, color))
                {
                    anim->update_hsv = true;
                    anim->hex_dirty = false;
                    format_hex_color(color, anim->hex_input_buf, sizeof(anim->hex_input_buf));
                }
            }
            anim->hex_focused = ImGui::IsItemActive();
            if (!anim->hex_focused && anim->hex_dirty)
            {
                if (parse_hex_color(anim->hex_input_buf, color))
                    anim->update_hsv = true;
                format_hex_color(color, anim->hex_input_buf, sizeof(anim->hex_input_buf));
                anim->hex_dirty = false;
            }
            gui->pop_font();

            ImGui::PopStyleColor(5);
            gui->pop_var(3);

            ImVec2 copy_cursor = ImVec2(input_rect.Max.x + button_gap, row_pos.y);
            ImGui::SetCursorScreenPos(copy_cursor);
            {
                ImRect copy_rect(copy_cursor, ImVec2(copy_cursor.x + button_width, copy_cursor.y + button_height));
                bool copy_hovered = copy_rect.Contains(ImGui::GetMousePos());
                bool copy_held = copy_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
                bool copy_clicked = copy_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

                gui->easing(anim->copy_hover_alpha, copy_hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, static_easing);
                gui->easing(anim->copy_active_alpha, copy_held ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing);

                ImVec4 copy_bg = ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.95f);
                draw->rect_filled(gui->window_drawlist(), copy_rect.Min, copy_rect.Max,
                    draw->get_clr(copy_bg), SCALE(5.0f));

                if (anim->copy_hover_alpha > 0.01f || anim->copy_active_alpha > 0.01f)
                {
                    ImVec4 accent = ImVec4(100.0f / 255.0f, 80.0f / 255.0f, 130.0f / 255.0f, 0.15f * anim->copy_hover_alpha + 0.25f * anim->copy_active_alpha);
                    draw->rect_filled(gui->window_drawlist(), copy_rect.Min, copy_rect.Max,
                        draw->get_clr(accent), SCALE(5.0f));
                }

                draw->rect(gui->window_drawlist(), copy_rect.Min, copy_rect.Max,
                    draw->get_clr(ImVec4(0.0f, 0.0f, 0.0f, 0.35f)), SCALE(5.0f), 0, SCALE(1.0f));

                ImFont* btn_font = font->get(main_font_data, menu_typography::k_control);
                if (btn_font)
                {
                    const char* copy_text = "Copy";
                    ImVec2 text_size = btn_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, copy_text);
                    ImVec2 text_pos = ImVec2(
                        copy_rect.Min.x + (button_width - text_size.x) * 0.5f,
                        copy_rect.Min.y + (button_height - text_size.y) * 0.5f
                    );

                    ImVec4 text_col = ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
                    draw->text(gui->window_drawlist(), btn_font, menu_typography::k_control, text_pos,
                        draw->get_clr(text_col), copy_text, nullptr);
                }

                gui->invisible_button("##copy_btn", ImVec2(button_width, button_height));
                if (copy_clicked)
                {
                    ImGui::SetClipboardText(hex_buffer);
                }
            }

            ImVec2 paste_cursor = ImVec2(copy_cursor.x + button_width + button_gap, row_pos.y);
            ImGui::SetCursorScreenPos(paste_cursor);
            {
                ImRect paste_rect(paste_cursor, ImVec2(paste_cursor.x + button_width, paste_cursor.y + button_height));
                bool paste_hovered = paste_rect.Contains(ImGui::GetMousePos());
                bool paste_held = paste_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
                bool paste_clicked = paste_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

                gui->easing(anim->paste_hover_alpha, paste_hovered ? 1.0f : 0.0f, menu_motion::k_control_hover, static_easing);
                gui->easing(anim->paste_active_alpha, paste_held ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing);

                ImVec4 paste_bg = ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.95f);
                draw->rect_filled(gui->window_drawlist(), paste_rect.Min, paste_rect.Max,
                    draw->get_clr(paste_bg), SCALE(5.0f));

                if (anim->paste_hover_alpha > 0.01f || anim->paste_active_alpha > 0.01f)
                {
                    ImVec4 accent = ImVec4(100.0f / 255.0f, 80.0f / 255.0f, 130.0f / 255.0f, 0.15f * anim->paste_hover_alpha + 0.25f * anim->paste_active_alpha);
                    draw->rect_filled(gui->window_drawlist(), paste_rect.Min, paste_rect.Max,
                        draw->get_clr(accent), SCALE(5.0f));
                }

                draw->rect(gui->window_drawlist(), paste_rect.Min, paste_rect.Max,
                    draw->get_clr(ImVec4(0.0f, 0.0f, 0.0f, 0.35f)), SCALE(5.0f), 0, SCALE(1.0f));

                ImFont* btn_font = font->get(main_font_data, menu_typography::k_control);
                if (btn_font)
                {
                    const char* paste_text = "Paste";
                    ImVec2 text_size = btn_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, paste_text);
                    ImVec2 text_pos = ImVec2(
                        paste_rect.Min.x + (button_width - text_size.x) * 0.5f,
                        paste_rect.Min.y + (button_height - text_size.y) * 0.5f
                    );

                    ImVec4 text_col = ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
                    draw->text(gui->window_drawlist(), btn_font, menu_typography::k_control, text_pos,
                        draw->get_clr(text_col), paste_text, nullptr);
                }

                gui->invisible_button("##paste_btn", ImVec2(button_width, button_height));
                if (paste_clicked)
                {
                    const char* clipboard = ImGui::GetClipboardText();
                    if (clipboard)
                    {
                        std::string paste_str = clipboard;
                        if (parse_hex_color(paste_str, color))
                        {
                            anim->update_hsv = true;
                            anim->hex_dirty = false;
                            format_hex_color(color, anim->hex_input_buf, sizeof(anim->hex_input_buf));
                        }
                    }
                }
            }
        }
        const ImVec2 actual_win_min = gui->window_pos();
        const ImVec2 actual_win_max = ImVec2(actual_win_min.x + gui->window_width(), actual_win_min.y + gui->window_height());
        menu_interaction::mark_welcome_overlay(ImRect(actual_win_min, actual_win_max));
        gui->end();
        gui->pop_color(2);
        gui->pop_var(5);

        const float pad = SCALE(6.0f);
        const ImRect actual_popup_rect(
            ImVec2(actual_win_min.x - pad, actual_win_min.y - pad),
            ImVec2(actual_win_max.x + pad, actual_win_max.y + pad));
        if (anim->opened && menu_interaction::input_block_clicked_outside(
            popup_name,
            popup_name + "_catch",
            { actual_popup_rect, box_rect }))
        {
            anim->opened = false;
        }
    }
    
    if (value_changed)
    {
        gui->hsv_to_rgb(anim->h, anim->s, anim->v, color[0], color[1], color[2]);
        if (anim->s > 0.0f)
            anim->saved_hue = anim->h;
    }

    anim->last_color = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
    
    
    if (!hide_label)
        render_tooltip(name, is_hovered);
    
    
    gui->item_size(full_rect);
    gui->item_add(full_rect, id);
    menu_search::focus_item_if_requested(name);
}
