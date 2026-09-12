#include "../headers/functions.h"
#include "../headers/widgets.h"

void c_widgets::section_header(std::string_view name)
{
    ImGuiWindow* window = gui->get_window();
    if (!window)
        return;

    ImFont* header_font = font->get(main_font_data, menu_typography::k_control);
    if (!header_font)
        return;

    ImVec2 pos = window->DC.CursorPos;
    pos.y += SCALE(2.0f);

    ImVec4 header_color = ImVec4(0.72f, 0.72f, 0.78f, 1.0f);
    ImDrawList* dl = gui->window_drawlist();
    dl->AddText(header_font, menu_typography::k_control, pos, draw->get_clr(header_color), name.data());
    const ImVec2 text_size = header_font->CalcTextSizeA(menu_typography::k_control, FLT_MAX, 0.0f, name.data(), gui->text_end(name.data()));
    ImRect full_rect(pos, ImVec2(pos.x + gui->content_avail().x, pos.y + text_size.y + SCALE(2.0f)));
    gui->item_size(full_rect);
    window->DC.CursorPos.y += SCALE(0.0f);
}

void c_widgets::separator_line()
{
    ImGuiWindow* window = gui->get_window();
    if (!window)
        return;

    ImVec2 pos = window->DC.CursorPos;
    pos.y += SCALE(4.0f);

    float line_width = gui->content_avail().x;
    ImVec4 line_color = ImVec4(0.28f, 0.28f, 0.33f, 0.35f);

    ImDrawList* dl = gui->window_drawlist();
    dl->AddLine(
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + line_width, pos.y),
        draw->get_clr(line_color),
        SCALE(1.0f)
    );

    ImRect full_rect(pos, ImVec2(pos.x + line_width, pos.y + SCALE(4.0f)));
    gui->item_size(full_rect);
}

void c_widgets::spacing(float height)
{
    ImGuiWindow* window = gui->get_window();
    if (!window)
        return;

    ImVec2 pos = window->DC.CursorPos;
    ImRect spacing_rect(pos, ImVec2(pos.x + 1.0f, pos.y + SCALE(height)));
    gui->item_size(spacing_rect);
}
