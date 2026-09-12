#include "../headers/functions.h"
#include "../headers/widgets.h"

void c_widgets::draw_background_pattern(ImDrawList* draw_list, ImVec2 content_start, ImVec2 content_end)
{
    if (!draw_list)
        return;

    draw_list->PushClipRect(content_start, content_end, true);

    const ImU32 top = IM_COL32(22, 18, 28, 40);
    const ImU32 bottom = IM_COL32(12, 12, 16, 18);
    draw_list->AddRectFilledMultiColor(content_start, content_end, top, top, bottom, bottom);

    const float w = content_end.x - content_start.x;
    const float h = content_end.y - content_start.y;
    const float step = SCALE(64.0f);
    const ImU32 line = IM_COL32(166, 102, 242, 18);

    for (float x = content_start.x; x < content_end.x; x += step)
        draw_list->AddLine(ImVec2(x, content_start.y), ImVec2(x, content_end.y), line, 1.0f);
    for (float y = content_start.y; y < content_end.y; y += step)
        draw_list->AddLine(ImVec2(content_start.x, y), ImVec2(content_end.x, y), line, 1.0f);

    draw_list->AddCircleFilled(
        ImVec2(content_start.x + w * 0.18f, content_start.y + h * 0.22f),
        SCALE(90.0f), IM_COL32(166, 102, 242, 12), 32);
    draw_list->AddCircleFilled(
        ImVec2(content_start.x + w * 0.82f, content_start.y + h * 0.72f),
        SCALE(110.0f), IM_COL32(120, 90, 180, 10), 32);

    draw_list->PopClipRect();
}
