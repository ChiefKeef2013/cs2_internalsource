#include "../headers/functions.h"
#include "../headers/widgets.h"
#include <algorithm>

void c_spectatorlist::add_spectator(const std::string& name)
{
    for (auto& spec : spectators)
    {
        if (spec.name != name)
            continue;
        spec.active = true;
        return;
    }

    spectator_item item;
    item.name = name;
    item.active = true;
    item.appear_alpha = 0.0f;
    spectators.push_back(item);
}

void c_spectatorlist::remove_spectator(const std::string& name)
{
    for (auto& spec : spectators)
    {
        if (spec.name == name)
            spec.active = false;
    }
}

void c_spectatorlist::clear_spectators()
{
    spectators.clear();
}

void c_spectatorlist::begin_sync()
{
    for (auto& spec : spectators)
        spec.active = false;
}

void c_spectatorlist::end_sync()
{
    spectators.erase(
        std::remove_if(spectators.begin(), spectators.end(),
            [](const spectator_item& s) { return !s.active && s.appear_alpha <= 0.01f; }),
        spectators.end());
}

void c_spectatorlist::render()
{
    if (!gui || !draw || !font)
        return;

    float spawn_alpha = var->gui.spectator_list_spawn_alpha;
    if (spawn_alpha <= 0.01f)
        return;

    float anim_alpha = spawn_alpha;
    float scale      = 0.85f + (spawn_alpha * 0.15f);

    try
    {
        ImGuiIO&    io        = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();

        ImFont* text_font = font->get(main_font_data, 13.0f);
        ImFont* icon_font = font->get(main_font_data, 14.0f);
        if (!text_font || !icon_font) return;

        float header_height = SCALE(28.0f);
        float item_height   = SCALE(26.0f);
        float padding       = SCALE(12.0f);
        float width         = SCALE(200.0f);

        float visible_count = 0.0f;
        for (auto& spec : spectators)
        {
            gui->easing(spec.appear_alpha, spec.active ? 1.0f : 0.0f, 10.0f, dynamic_easing);
            if (spec.appear_alpha > 0.01f)
                visible_count += spec.appear_alpha;
        }

        const bool menu_open = var->gui.menu_open;
        const bool show_empty = menu_open && visible_count <= 0.01f;
        if (visible_count <= 0.01f && !menu_open)
            return;
        if (show_empty)
            visible_count = 1.0f;

        float total_height = header_height + visible_count * item_height + SCALE(8.0f);

        ImVec2 pos  = spectator_pos;
        ImVec2 size = ImVec2(width, total_height);
        
        
        ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        ImVec2 scaled_pos = ImVec2(
            center.x + (pos.x - center.x) * scale,
            center.y + (pos.y - center.y) * scale
        );
        ImVec2 scaled_size = ImVec2(size.x * scale, size.y * scale);

        ImRect header_rect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + header_height * scale));
        bool   header_hovered = header_rect.Contains(ImGui::GetMousePos());
        ImRect panel_rect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y));

        if (menu_open && (header_hovered || (show_empty && panel_rect.Contains(ImGui::GetMousePos()))) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            is_dragging = true;
            drag_offset = ImVec2(ImGui::GetMousePos().x - pos.x,
                                 ImGui::GetMousePos().y - pos.y);
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            is_dragging = false;

        if (is_dragging)
        {
            spectator_pos.x = ImClamp(ImGui::GetMousePos().x - drag_offset.x,
                                      0.0f, io.DisplaySize.x - width);
            spectator_pos.y = ImClamp(ImGui::GetMousePos().y - drag_offset.y,
                                      0.0f, io.DisplaySize.y - total_height);
            pos = spectator_pos;
            
            
            center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
            scaled_pos = ImVec2(
                center.x + (pos.x - center.x) * scale,
                center.y + (pos.y - center.y) * scale
            );
            scaled_size = ImVec2(size.x * scale, size.y * scale);
        }

        float shadow_blur = SCALE(12.0f);
        for (int s = 0; s < 6; s++)
        {
            float p  = (float)s / 6.0f;
            float a  = (1.0f - p) * 0.25f * anim_alpha;
            float bl = shadow_blur * p;
            draw_list->AddRectFilled(
                ImVec2(scaled_pos.x - bl, scaled_pos.y - bl),
                ImVec2(scaled_pos.x + scaled_size.x + bl, scaled_pos.y + scaled_size.y + bl),
                IM_COL32(0, 0, 0, (int)(a * 255)),
                SCALE(6.0f) + bl);
        }

        ImVec4 bg_color = ImVec4(31.0f/255.0f, 31.0f/255.0f, 36.0f/255.0f, 0.95f * anim_alpha);
        draw_list->AddRectFilled(
            scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
            draw->get_clr(bg_color),
            SCALE(6.0f));

        
        ImVec4 outline_color = ImVec4(0.0f, 0.0f, 0.0f, 0.8f * anim_alpha);
        draw_list->AddRect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
            draw->get_clr(outline_color), SCALE(6.0f), 0, SCALE(1.5f));

        ImVec4 header_bg = ImVec4(35.0f/255.0f, 35.0f/255.0f, 40.0f/255.0f, 0.95f * anim_alpha);
        draw_list->AddRectFilled(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + header_height * scale),
            draw->get_clr(header_bg), SCALE(6.0f), ImDrawFlags_RoundCornersTop);

        ImVec4 header_outline = ImVec4(0.0f, 0.0f, 0.0f, 0.6f * anim_alpha);
        draw_list->AddRect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + header_height * scale),
            draw->get_clr(header_outline), SCALE(6.0f), ImDrawFlags_RoundCornersTop, SCALE(1.0f));

        const char* hdr_icon = "\xEF\x81\x9C"; 
        ImVec2      hdr_icon_sz = icon_font->CalcTextSizeA(14.f, FLT_MAX, 0, hdr_icon);
        draw_list->AddText(icon_font, 14.f,
            ImVec2(scaled_pos.x + SCALE(10) * scale, scaled_pos.y + (header_height * scale - hdr_icon_sz.y) * 0.5f),
            IM_COL32(179, 143, 228, (int)(255 * anim_alpha)), hdr_icon);

        const char* hdr_text = "spectators";
        ImVec2      hdr_text_sz = text_font->CalcTextSizeA(13.f, FLT_MAX, 0, hdr_text);
        draw_list->AddText(text_font, 13.f,
            ImVec2(scaled_pos.x + (SCALE(10) + hdr_icon_sz.x + SCALE(6)) * scale,
                   scaled_pos.y + (header_height * scale - hdr_text_sz.y) * 0.5f),
            IM_COL32(230, 230, 230, (int)(255 * anim_alpha)), hdr_text);

        float item_y = scaled_pos.y + header_height * scale + SCALE(4.f) * scale;
        int   drawn  = 0;

        float item_scaled_h = item_height * scale;
        float pad_scaled    = padding * scale;

        for (int idx = 0; idx < (int)spectators.size(); idx++)
        {
            auto& spec = spectators[idx];
            if (spec.appear_alpha <= 0.01f) continue;

            const float item_a = anim_alpha * spec.appear_alpha;
            const float row_h = item_scaled_h * spec.appear_alpha;
            ImRect item_rect(
                ImVec2(scaled_pos.x, item_y),
                ImVec2(scaled_pos.x + scaled_size.x, item_y + row_h));

            bool item_hovered = item_rect.Contains(ImGui::GetMousePos());

            if (item_hovered)
                draw_list->AddRectFilled(item_rect.Min, item_rect.Max,
                    IM_COL32(255, 255, 255, (int)(12 * item_a)));

            draw_list->AddText(text_font, 13.f,
                ImVec2(scaled_pos.x + pad_scaled, item_y + (row_h - SCALE(13.f)) * 0.5f),
                IM_COL32(180, 180, 185, (int)(255 * item_a)), spec.name.c_str());

            const char* eye_icon = "\xEF\x81\x9C";
            ImVec2 eye_sz = icon_font->CalcTextSizeA(12.f, FLT_MAX, 0, eye_icon);
            ImVec2 eye_pos = ImVec2(
                scaled_pos.x + scaled_size.x - pad_scaled - eye_sz.x,
                item_y + (row_h - eye_sz.y) * 0.5f);

            draw_list->AddText(icon_font, 12.f, eye_pos,
                IM_COL32(179, 143, 228, (int)(200 * item_a)), eye_icon);

            item_y += row_h;
            drawn++;
        }

        if (show_empty)
        {
            const char* empty_text = "no spectators";
            ImVec2 empty_sz = text_font->CalcTextSizeA(13.f, FLT_MAX, 0, empty_text);
            draw_list->AddText(text_font, 13.f,
                ImVec2(scaled_pos.x + pad_scaled, item_y + (item_scaled_h - empty_sz.y) * 0.5f),
                IM_COL32(150, 150, 158, (int)(220 * anim_alpha)), empty_text);
        }
    }
    catch (...)
    {
    }
}
