#include "../headers/functions.h"
#include "../headers/widgets.h"
#include <algorithm>

std::string c_keybinds::vk_to_string(int vk)
{
    if (vk == 0)             return "NONE";
    if (vk == VK_LBUTTON)   return "M1";
    if (vk == VK_RBUTTON)   return "M2";
    if (vk == VK_MBUTTON)   return "M3";
    if (vk == VK_XBUTTON1)  return "M4";
    if (vk == VK_XBUTTON2)  return "M5";
    if (vk == VK_SPACE)     return "SPACE";
    if (vk == VK_RETURN)    return "ENTER";
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) return "SHIFT";
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) return "CTRL";
    if (vk == VK_MENU)      return "ALT";
    if (vk == VK_DELETE)    return "DEL";
    if (vk == VK_INSERT)    return "INS";
    if (vk == VK_HOME)      return "HOME";
    if (vk == VK_END)       return "END";
    if (vk == VK_PRIOR)     return "PGUP";
    if (vk == VK_NEXT)      return "PGDN";
    if (vk == VK_ESCAPE)    return "ESC";
    if (vk == VK_TAB)       return "TAB";
    if (vk == VK_CAPITAL)   return "CAPS";

    
    if (vk >= VK_F1 && vk <= VK_F12)
    {
        char buf[8];
        sprintf_s(buf, "F%d", vk - VK_F1 + 1);
        return buf;
    }

    
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
    {
        char buf[8];
        sprintf_s(buf, "NUM%d", vk - VK_NUMPAD0);
        return buf;
    }

    
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
    {
        char buf[4] = { (char)vk, 0 };
        return buf;
    }

    char buf[32];
    sprintf_s(buf, "KEY%d", vk);
    return buf;
}

void c_keybinds::clear_keybinds()
{
    keybinds.clear();
}

void c_keybinds::begin_sync()
{
    for (auto& kb : keybinds)
        kb.active = false;
}

void c_keybinds::end_sync()
{
    keybinds.erase(
        std::remove_if(keybinds.begin(), keybinds.end(),
            [](const keybind_item& kb) { return !kb.active && kb.appear_alpha <= 0.01f; }),
        keybinds.end());
}

void c_keybinds::add_keybind(const std::string& name, int key,
                              keybind_mode mode, bool active, bool state)
{
    for (auto& kb : keybinds)
    {
        if (kb.name != name)
            continue;

        kb.key = key;
        kb.mode = mode;
        kb.active = active;
        kb.state = state || (mode == keybind_mode::always);
        return;
    }

    keybind_item item;
    item.name = name;
    item.key = key;
    item.mode = mode;
    item.active = active;
    item.state = state || (mode == keybind_mode::always);
    item.appear_alpha = 0.0f;
    keybinds.push_back(item);
}

bool c_keybinds::is_active(const std::string& name)
{
    for (auto& kb : keybinds)
        if (kb.name == name) return kb.state;
    return false;
}

void c_keybinds::update()
{
    for (auto& kb : keybinds)
    {
        
        if (kb.listening)
        {
            
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                kb.listening = false;
                continue;
            }

            
            static const int scan_keys[] = {
                VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2,
                VK_SPACE, VK_RETURN, VK_SHIFT, VK_CONTROL, VK_MENU,
                VK_DELETE, VK_INSERT, VK_HOME, VK_END, VK_PRIOR, VK_NEXT,
                VK_TAB, VK_CAPITAL,
                VK_F1,VK_F2,VK_F3,VK_F4,VK_F5,VK_F6,
                VK_F7,VK_F8,VK_F9,VK_F10,VK_F11,VK_F12,
                VK_NUMPAD0,VK_NUMPAD1,VK_NUMPAD2,VK_NUMPAD3,VK_NUMPAD4,
                VK_NUMPAD5,VK_NUMPAD6,VK_NUMPAD7,VK_NUMPAD8,VK_NUMPAD9,
            };
            for (int vk : scan_keys)
            {
                if (GetAsyncKeyState(vk) & 0x8000)
                {
                    kb.key       = vk;
                    kb.listening = false;
                    break;
                }
            }
            
            if (kb.listening)
            {
                for (int vk = '0'; vk <= 'Z'; vk++)
                {
                    if (GetAsyncKeyState(vk) & 0x8000)
                    {
                        kb.key       = vk;
                        kb.listening = false;
                        break;
                    }
                }
            }
            continue; 
        }

        
        switch (kb.mode)
        {
        case keybind_mode::always:
            kb.state = true;
            break;

        case keybind_mode::hold:
            if (kb.key != 0)
                kb.state = (GetAsyncKeyState(kb.key) & 0x8000) != 0;
            else
                kb.state = false;
            break;

        case keybind_mode::toggle:
        {
            if (kb.key != 0)
            {
                static std::unordered_map<std::string, bool> prev_down;
                bool down = (GetAsyncKeyState(kb.key) & 0x8000) != 0;
                if (down && !prev_down[kb.name])
                    kb.state = !kb.state;
                prev_down[kb.name] = down;
            }
            break;
        }
        }
    }
}

void c_keybinds::render()
{
    if (!gui || !draw || !font)
        return;

    float spawn_alpha = var->gui.active_hotkeys_spawn_alpha;
    if (spawn_alpha <= 0.01f)
        return;

    float anim_alpha  = spawn_alpha;
    float scale       = 0.85f + (spawn_alpha * 0.15f);

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
        for (auto& kb : keybinds)
        {
            gui->easing(kb.appear_alpha, kb.active ? 1.0f : 0.0f, 10.0f, dynamic_easing);
            if (kb.appear_alpha > 0.01f)
                visible_count += kb.appear_alpha;
        }

        const bool menu_open = var->gui.menu_open;
        const bool show_empty = menu_open && visible_count <= 0.01f;
        if (visible_count <= 0.01f && !menu_open)
            return;
        if (show_empty)
            visible_count = 1.0f;

        float total_height = header_height + visible_count * item_height + SCALE(8.0f);

        ImVec2 pos  = keybind_pos;
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
            keybind_pos.x = ImClamp(ImGui::GetMousePos().x - drag_offset.x,
                                    0.0f, io.DisplaySize.x - width);
            keybind_pos.y = ImClamp(ImGui::GetMousePos().y - drag_offset.y,
                                    0.0f, io.DisplaySize.y - total_height);
            pos = keybind_pos;
            
            
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

        const char* hdr_icon = "\xEF\x81\xB4"; 
        ImVec2      hdr_icon_sz = icon_font->CalcTextSizeA(14.f, FLT_MAX, 0, hdr_icon);
        draw_list->AddText(icon_font, 14.f,
            ImVec2(scaled_pos.x + SCALE(10) * scale, scaled_pos.y + (header_height * scale - hdr_icon_sz.y) * 0.5f),
            IM_COL32(179, 143, 228, (int)(255 * anim_alpha)), hdr_icon);

        const char* hdr_text = "active keybinds";
        ImVec2      hdr_text_sz = text_font->CalcTextSizeA(13.f, FLT_MAX, 0, hdr_text);
        draw_list->AddText(text_font, 13.f,
            ImVec2(scaled_pos.x + (SCALE(10) + hdr_icon_sz.x + SCALE(6)) * scale,
                   scaled_pos.y + (header_height * scale - hdr_text_sz.y) * 0.5f),
            IM_COL32(230, 230, 230, (int)(255 * anim_alpha)), hdr_text);

        float item_y = scaled_pos.y + header_height * scale + SCALE(4.f) * scale;
        int   drawn  = 0;

        float item_scaled_h = item_height * scale;
        float pad_scaled    = padding * scale;

        for (int idx = 0; idx < (int)keybinds.size(); idx++)
        {
            auto& kb = keybinds[idx];
            if (kb.appear_alpha <= 0.01f) continue;

            const float item_a = anim_alpha * kb.appear_alpha;
            const float row_h = item_scaled_h * kb.appear_alpha;
            ImRect item_rect(
                ImVec2(scaled_pos.x, item_y),
                ImVec2(scaled_pos.x + scaled_size.x, item_y + row_h));

            bool item_hovered = item_rect.Contains(ImGui::GetMousePos());

            if (item_hovered)
                draw_list->AddRectFilled(item_rect.Min, item_rect.Max,
                    IM_COL32(255, 255, 255, (int)(12 * item_a)));

            draw_list->AddText(text_font, 13.f,
                ImVec2(scaled_pos.x + pad_scaled, item_y + (row_h - SCALE(13.f)) * 0.5f),
                IM_COL32(180, 180, 185, (int)(255 * item_a)), kb.name.c_str());

            std::string key_str = kb.listening ? "..." : vk_to_string(kb.key);
            ImVec2 key_sz  = text_font->CalcTextSizeA(11.f, FLT_MAX, 0, key_str.c_str());
            float  btn_w   = key_sz.x + SCALE(12.f) * scale;
            float  btn_h   = SCALE(16.f) * scale;
            ImVec2 btn_pos = ImVec2(
                scaled_pos.x + scaled_size.x - pad_scaled - btn_w,
                item_y + (row_h - btn_h) * 0.5f);
            ImRect btn_rect(btn_pos, ImVec2(btn_pos.x + btn_w, btn_pos.y + btn_h));
            bool   btn_hovered = btn_rect.Contains(ImGui::GetMousePos());

            int a = (int)(item_a * 255);
            ImU32 btn_bg = kb.listening
                ? IM_COL32(179, 143, 228, (int)(60 * item_a))
                : (btn_hovered ? IM_COL32(179, 143, 228, (int)(40 * item_a)) : IM_COL32(255, 255, 255, (int)(18 * item_a)));
            draw_list->AddRectFilled(btn_rect.Min, btn_rect.Max, btn_bg, SCALE(3.f) * scale);
            draw_list->AddRect(btn_rect.Min, btn_rect.Max,
                kb.listening ? IM_COL32(179, 143, 228, (int)(180 * item_a)) : IM_COL32(255,255,255,(int)(40 * item_a)),
                SCALE(3.f) * scale, 0, SCALE(1.f));

            ImU32 key_clr = kb.listening
                ? IM_COL32(179, 143, 228, a)
                : IM_COL32(220, 220, 225, a);
            draw_list->AddText(text_font, 11.f,
                ImVec2(btn_pos.x + (btn_w - key_sz.x) * 0.5f,
                       btn_pos.y + (btn_h - SCALE(11.f)) * 0.5f),
                key_clr, key_str.c_str());

            
            if (btn_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                for (auto& o : keybinds) o.listening = false;
                kb.listening = true;
            }

            ImVec2 dot_pos = ImVec2(btn_pos.x - SCALE(10.f) * scale,
                                    item_y + item_scaled_h * 0.5f);
            ImU32 dot_clr;
            switch (kb.mode)
            {
            case keybind_mode::always: dot_clr = IM_COL32(100, 220, 130, (int)(200 * anim_alpha)); break;
            case keybind_mode::hold:   dot_clr = IM_COL32(100, 170, 255, (int)(200 * anim_alpha)); break;
            default:                   dot_clr = IM_COL32(179, 143, 228, (int)(200 * anim_alpha)); break;
            }
            draw_list->AddCircleFilled(dot_pos, SCALE(2.5f) * scale, dot_clr);

            if (item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                context_item = idx;

            item_y += row_h;
            drawn++;
        }

        if (show_empty)
        {
            const char* empty_text = "no active binds";
            ImVec2 empty_sz = text_font->CalcTextSizeA(13.f, FLT_MAX, 0, empty_text);
            draw_list->AddText(text_font, 13.f,
                ImVec2(scaled_pos.x + pad_scaled, item_y + (item_scaled_h - empty_sz.y) * 0.5f),
                IM_COL32(150, 150, 158, (int)(220 * anim_alpha)), empty_text);
        }

        if (context_item >= 0 && context_item < (int)keybinds.size())
        {
            auto& kb = keybinds[context_item];

            float  menu_item_h = SCALE(24.f);
            float  menu_w      = SCALE(140.f);
            float  menu_h      = menu_item_h * 3 + SCALE(8.f);
            ImVec2 mpos        = ImGui::GetMousePos();

            static ImVec2 ctx_pos = {};
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                ctx_pos = mpos;

            ctx_pos.x = ImMin(ctx_pos.x, io.DisplaySize.x - menu_w - SCALE(4));
            ctx_pos.y = ImMin(ctx_pos.y, io.DisplaySize.y - menu_h - SCALE(4));

            ImRect menu_rect(ctx_pos, ImVec2(ctx_pos.x + menu_w, ctx_pos.y + menu_h));

            for (int s = 0; s < 5; s++)
            {
                float p  = (float)s / 5.f;
                float bl = SCALE(10.f) * p;
                draw_list->AddRectFilled(
                    ImVec2(ctx_pos.x - bl, ctx_pos.y - bl),
                    ImVec2(ctx_pos.x + menu_w + bl, ctx_pos.y + menu_h + bl),
                    IM_COL32(0, 0, 0, (int)((1-p)*0.4f*255)),
                    SCALE(6.f));
            }

            draw_list->AddRectFilled(ctx_pos,
                ImVec2(ctx_pos.x + menu_w, ctx_pos.y + menu_h),
                IM_COL32(18, 18, 22, 245), SCALE(6.f));
            draw_list->AddRect(ctx_pos,
                ImVec2(ctx_pos.x + menu_w, ctx_pos.y + menu_h),
                IM_COL32(179, 143, 228, 50), SCALE(6.f), 0, SCALE(1.f));

            const char* labels[3] = { "Toggle", "Hold", "Always" };
            keybind_mode modes[3] = { keybind_mode::toggle, keybind_mode::hold, keybind_mode::always };
            ImU32 dot_colors[3]   = {
                IM_COL32(179, 143, 228, 255),
                IM_COL32(100, 170, 255, 255),
                IM_COL32(100, 220, 130, 255),
            };

            for (int m = 0; m < 3; m++)
            {
                ImVec2 mitem_pos = ImVec2(ctx_pos.x, ctx_pos.y + SCALE(4.f) + m * menu_item_h);
                ImRect mitem_rect(mitem_pos,
                    ImVec2(mitem_pos.x + menu_w, mitem_pos.y + menu_item_h));
                bool mitem_hovered = mitem_rect.Contains(ImGui::GetMousePos());

                if (mitem_hovered)
                    draw_list->AddRectFilled(mitem_rect.Min, mitem_rect.Max,
                        IM_COL32(255, 255, 255, 15), SCALE(4.f));

                
                draw_list->AddCircleFilled(
                    ImVec2(mitem_pos.x + SCALE(14.f), mitem_pos.y + menu_item_h * 0.5f),
                    SCALE(3.f), dot_colors[m]);

                bool is_active_mode = (kb.mode == modes[m]);
                ImU32 lbl_clr = is_active_mode
                    ? IM_COL32(179, 143, 228, 255)
                    : IM_COL32(200, 200, 205, 200);
                draw_list->AddText(text_font, 13.f,
                    ImVec2(mitem_pos.x + SCALE(26.f),
                           mitem_pos.y + (menu_item_h - SCALE(13.f)) * 0.5f),
                    lbl_clr, labels[m]);

                if (is_active_mode)
                {
                    const char* check = "\xEF\x80\x8C"; 
                    ImVec2 check_sz = text_font->CalcTextSizeA(11.f, FLT_MAX, 0, check);
                    draw_list->AddText(text_font, 11.f,
                        ImVec2(mitem_pos.x + menu_w - SCALE(14.f) - check_sz.x * 0.5f,
                               mitem_pos.y + (menu_item_h - SCALE(11.f)) * 0.5f),
                        IM_COL32(179, 143, 228, 255), check);
                }

                if (mitem_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    kb.mode  = modes[m];
                    if (kb.mode == keybind_mode::always)
                        kb.state = true;
                    else if (kb.mode == keybind_mode::hold)
                        kb.state = false;
                    context_item = -1;
                }
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !menu_rect.Contains(ImGui::GetMousePos()))
                context_item = -1;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !menu_rect.Contains(ImGui::GetMousePos()))
                context_item = -1;
        }
    }
    catch (...) {}
}
