#pragma once
#include "includes.h"
#include <unordered_map>
#include <initializer_list>
#include "widgets.h"
#include "../headers/functions.h"
#include <cctype>

using namespace ImGui;

#define SCALE(...) scale_impl(__VA_ARGS__, var->gui.dpi)

inline ImVec2 scale_impl(const ImVec2& vec, float dpi) {
    return ImVec2(roundf(vec.x * dpi), roundf(vec.y * dpi));
}

inline ImVec2 scale_impl(float x, float y, float dpi) {
    return ImVec2(roundf(x * dpi), roundf(y * dpi));
}

inline float scale_impl(float var, float dpi) {
    return roundf(var * dpi);
}

enum positions
{
    pos_all,
    pos_x,
    pos_y
};

enum easing_type
{
    static_easing,
    dynamic_easing
};

namespace menu_theme
{
    inline constexpr float k_window_rounding = 12.0f;
    inline constexpr float k_panel_rounding = 10.0f;
    inline constexpr float k_control_rounding = 6.0f;
    inline constexpr float k_header_height = 30.0f;
    inline constexpr float k_compact_spacing = 1.0f;
    inline constexpr float k_section_gap = 6.0f;

    inline ImVec4 window_bg(float alpha = 1.0f) { return ImVec4(0.12f, 0.12f, 0.14f, 0.95f * alpha); }
    inline ImVec4 window_bar(float alpha = 1.0f) { return ImVec4(0.15f, 0.15f, 0.18f, 0.95f * alpha); }
    inline ImVec4 panel_bg(float alpha = 1.0f) { return ImVec4(0.15f, 0.15f, 0.18f, 0.97f * alpha); }
    inline ImVec4 panel_bg_soft(float alpha = 1.0f) { return ImVec4(0.17f, 0.17f, 0.20f, 0.98f * alpha); }
    inline ImVec4 border(float alpha = 1.0f) { return ImVec4(0.30f, 0.30f, 0.35f, 0.82f * alpha); }
    inline ImVec4 border_soft(float alpha = 1.0f) { return ImVec4(0.24f, 0.24f, 0.28f, 0.72f * alpha); }
    inline ImVec4 accent(float alpha = 1.0f) { return ImVec4(0.62f, 0.49f, 0.82f, alpha); }
    inline ImVec4 accent_soft(float alpha = 1.0f) { return ImVec4(0.45f, 0.36f, 0.60f, alpha); }
    inline ImVec4 text(float alpha = 1.0f) { return ImVec4(0.90f, 0.90f, 0.93f, alpha); }
    inline ImVec4 text_muted(float alpha = 1.0f) { return ImVec4(0.68f, 0.68f, 0.74f, alpha); }
}

namespace menu_motion
{
    inline constexpr float k_menu_open = 8.5f;
    inline constexpr float k_popup_open = 13.0f;
    inline constexpr float k_popup_close = 13.0f;
    inline constexpr float k_content_fade = 10.0f;
    inline constexpr float k_control_hover = 14.0f;
    inline constexpr float k_control_active = 12.0f;
    inline constexpr float k_control_scroll = 18.0f;
}

namespace menu_typography
{
    inline constexpr float k_control = 13.0f;
    inline constexpr float k_button = 13.0f;
    inline constexpr float k_icon = 14.0f;
}

namespace menu_interaction
{
    inline void close_active_popup()
    {
        if (!var)
            return;

        var->gui.active_popup_id = 0;
    }

    inline void close_popups_for_navigation()
    {
        if (!var)
            return;

        var->gui.active_popup_id = 0;
        ++var->gui.popup_close_epoch;
    }

    inline bool popup_outside_clicked(const ImRect& popup_rect, int opened_frame)
    {
        if (!var)
            return false;

        if (ImGui::GetFrameCount() <= opened_frame)
            return false;

        if (var->gui.dropdown_blocks_input || var->gui.color_picker_open || var->gui.color_picker_open_prev)
            return false;

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            return false;

        return !popup_rect.Contains(ImGui::GetMousePos());
    }

    inline bool nested_popup_interaction_active()
    {
        return var &&
            (var->gui.popup_blocks_input_prev ||
             var->gui.popup_blocks_input ||
             var->gui.dropdown_blocks_input_prev ||
             var->gui.dropdown_blocks_input ||
             var->gui.color_picker_open_prev ||
             var->gui.color_picker_open);
    }

    inline bool input_block_clicked_outside(
        const std::string& window_id,
        const std::string& catch_id,
        std::initializer_list<ImRect> keep_open_rects)
    {
        (void)window_id;
        (void)catch_id;

        const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if (!clicked)
            return false;

        const ImVec2 mouse = ImGui::GetMousePos();
        for (const ImRect& rect : keep_open_rects)
        {
            if (rect.Contains(mouse))
                return false;
        }

        return true;
    }

    inline ImRect popup_container_rect()
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float margin = SCALE(8.0f);
        return ImRect(
            ImVec2(margin, margin),
            ImVec2((std::max)(margin + 1.0f, display.x - margin), (std::max)(margin + 1.0f, display.y - margin)));
    }

    inline void mark_welcome_overlay(const ImRect& rect)
    {
        if (!var)
            return;

        if (!var->gui.overlay_blocks_welcome)
        {
            var->gui.overlay_blocks_welcome = true;
            var->gui.overlay_block_min = rect.Min;
            var->gui.overlay_block_max = rect.Max;
            return;
        }

        var->gui.overlay_block_min.x = (std::min)(var->gui.overlay_block_min.x, rect.Min.x);
        var->gui.overlay_block_min.y = (std::min)(var->gui.overlay_block_min.y, rect.Min.y);
        var->gui.overlay_block_max.x = (std::max)(var->gui.overlay_block_max.x, rect.Max.x);
        var->gui.overlay_block_max.y = (std::max)(var->gui.overlay_block_max.y, rect.Max.y);
    }

    inline ImVec2 popup_position_near_rect(const ImRect& trigger_rect, const ImVec2& popup_size, float gap = 0.0f)
    {
        const float resolved_gap = gap > 0.0f ? gap : SCALE(8.0f);
        const ImRect container = popup_container_rect();
        const float room_right = container.Max.x - trigger_rect.Max.x;
        const float room_left = trigger_rect.Min.x - container.Min.x;

        float x = trigger_rect.Max.x + resolved_gap;
        if (room_right < popup_size.x && room_left > room_right)
            x = trigger_rect.Min.x - popup_size.x - resolved_gap;

        x = ImClamp(x, container.Min.x, container.Max.x - popup_size.x);

        float y = trigger_rect.Min.y;
        if (y + popup_size.y > container.Max.y)
            y = container.Max.y - popup_size.y;
        if (y < container.Min.y)
            y = container.Min.y;

        return ImVec2(x, y);
    }
}

namespace menu_search
{
    inline bool has_visible_label(std::string_view label)
    {
        return !label.empty() && !(label.size() >= 2 && label[0] == '#' && label[1] == '#');
    }

    inline bool equals_ignore_case(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;

        for (std::size_t i = 0; i < lhs.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                std::tolower(static_cast<unsigned char>(rhs[i])))
            {
                return false;
            }
        }

        return true;
    }

    inline void focus_item_if_requested(std::string_view label)
    {
        if (!var || !var->gui.search_navigation_pending || var->gui.search_navigate_to.empty())
            return;

        if (!has_visible_label(label) || !equals_ignore_case(label, var->gui.search_navigate_to))
            return;

        ImGui::SetScrollHereY(0.35f);
        var->gui.search_navigation_found = true;
        var->gui.search_navigation_pending = false;
    }
}

class c_gui
{
public:
    std::unordered_map<ImGuiID, void*> m_anim_states;

    template <typename T>
    T* anim_container(ImGuiID id)
    {
        auto it = m_anim_states.find(id);
        if (it != m_anim_states.end())
            return static_cast<T*>(it->second);

        T* new_state = new T();
        m_anim_states[id] = new_state;
        return new_state;
    }

    float fixed_speed(float speed) { return speed / ImGui::GetIO().Framerate; }

    template<typename T>
    T easing(T& value, T val, float speed, int type, bool dynamic_round = false)
    {
        if (type == static_easing)
        {
            if constexpr (std::is_same<T, ImVec4>::value)
            {
                return { 1.f, 1.f, 1.f, 1.f };
            }
            else
            {
                T step = fixed_speed(speed);

                if (value < val)
                {
                    value += step;
                    if (value > val) value = val;
                }
                else if (value > val)
                {
                    value -= step;
                    if (value < val) value = val;
                }
            }
        }
        else if (type == dynamic_easing)
        {
            if constexpr (std::is_same<T, ImVec4>::value)
            {
                value = ImLerp(value, val, fixed_speed(speed));
            }
            else
                value = ImLerp(value, val + (dynamic_round ? 0.5f : 0.f), fixed_speed(speed));
        }

        return value;
    }

    bool begin(std::string_view name, bool* p_open = nullptr, window_flags flags = window_flags_none);

    void end();

    void push_color(style_col idx, ImU32 col);

    void pop_color(int count = 1);

    void push_var(style_var idx, float val);

    void push_var(style_var idx, const ImVec2& val);

    void pop_var(int count = 1);

    void push_font(ImFont* font);

    void pop_font();

    void set_pos(const ImVec2& pos, int type);

    void set_pos(float pos, int type);

    ImVec2 get_pos();

    void set_screen_pos(const ImVec2& pos, int type);

    void set_screen_pos(float pos, int type);

    ImVec2 get_screen_pos();

    void begin_group();

    void end_group();

    void begin_content(std::string_view id, const ImVec2& size, const ImVec2& padding = ImVec2(0, 0), const ImVec2& spacing = ImVec2(0, 0), window_flags window_flags__ = 0, child_flags child_flags__ = 0);

    void end_content();

    void sameline(float offset_from_start_x = 0.f, float spacing_w = -1.f);

    void dummy(const ImVec2& size);

    bool begin_def_child(std::string_view name, const ImVec2& size_arg = ImVec2(0, 0), child_flags child_flags = 0, window_flags window_flags = 0);

    void end_def_child();

    void set_next_window_pos(const ImVec2& pos, gui_cond cond = 0, const ImVec2& pivot = ImVec2(0, 0));

    void set_next_window_size(const ImVec2& size, gui_cond cond = 0);

    ImVec2 text_size(ImFont* font, const char* text, const char* text_end = nullptr, bool hide_text_after_double_hash = false, float wrap_width = -1.f);

    ImVec2 window_size();

    float window_width();

    float window_height();

    ImDrawList* window_drawlist();

    ImDrawList* foreground_drawlist();

    ImDrawList* background_drawlist();

    ImVec2 window_pos();

    ImGuiWindow* get_window();

    void push_id(const char* str_id);

    void push_id(const char* str_id_begin, const char* str_id_end);

    void push_id(const void* ptr_id);

    void push_id(int int_id);

    void pop_id();
    
    ImGuiID get_id(const char* str, const char* str_end);

    ImGuiID get_id(const void* ptr);

    ImGuiID get_id(int n);

    ImVec2 content_avail();

    ImVec2 content_max();

    void item_size(const ImVec2& size, float text_baseline_y = -1.f);

    void item_size(const ImRect& bb, float text_baseline_y = -1.f);

    bool item_add(const ImRect& bb, ImGuiID id, const ImRect* nav_bb_arg = NULL, ImGuiItemFlags extra_flags = 0);

    bool is_window_hovered(ImGuiHoveredFlags flags);

    bool is_window_focused(ImGuiFocusedFlags flags);

    void set_window_focus();

    void set_window_focus(const char* name);

    bool is_rect_visible(const ImVec2& size);

    bool is_rect_visible(const ImRect& rect);

    ImVec2 adjust_window_pos(const ImVec2& rect, const ImVec2& window_size);

    bool button_behavior(const ImRect& bb, ImGuiID id, bool* out_hovered, bool* out_held, ImGuiButtonFlags flags = 0);

    bool invisible_button(const char* str_id, const ImVec2& size_arg, mouse_button flags = 0);

    ImVec4 u32_to_float4(ImU32 in);

    ImU32 float4_to_u32(const ImVec4& in);

    void rgb_to_hsv(float r, float g, float b, float& out_h, float& out_s, float& out_v);

    void hsv_to_rgb(float h, float s, float v, float& out_r, float& out_g, float& out_b);

    bool item_hoverable(const ImRect& bb, ImGuiID id, ImGuiItemFlags item_flags = 0);

    bool is_item_hovered(ImGuiHoveredFlags flags);

    bool is_item_active();

    bool is_item_clicked(mouse_button mouse_button);

    bool mouse_down(mouse_button button);

    bool mouse_clicked(mouse_button button, bool repeat = false);

    bool mouse_released(mouse_button button);

    bool mouse_double_clicked(mouse_button button);
    
    const char* text_end(const char* text);

    template<typename T>
    const char* get_fmt(char* text, T* value, std::string_view fmt)
    {
        ImFormatString(text, IM_ARRAYSIZE(text), fmt.data(), *value);
        return text + strlen(text);
    }

    bool is_window_cond(ImGuiWindow* window, const std::vector<std::string>& names);

    ImVec2 mouse_pos();
    
    void set_style();

    void draw_decorations();

    void initialize();

    void render();
};

inline std::unique_ptr<c_gui> gui = std::make_unique<c_gui>();

namespace menu_content {

	void draw_visuals( int subtab );
	void draw_aim( int subtab );
	void draw_misc( int subtab );
	void draw_self( int subtab );
	void draw_settings( int subtab );

	void sync_keybinds( );
	void sync_spectators( );

}
