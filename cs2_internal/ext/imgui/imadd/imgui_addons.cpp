#include "imgui_addons.h"

#include "../imgui.h"
#include "../imgui_internal.h"

#include <map>
#include <string>
#include <cmath>
#include <cstdio>
#include <Windows.h>

using namespace ImGui;

ImVec4 ImAdd::HexToColorVec4(unsigned int hex_color, float alpha)
{
    ImVec4 color;

    color.x = ((hex_color >> 16) & 0xFF) / 255.0f;
    color.y = ((hex_color >> 8) & 0xFF) / 255.0f;
    color.z = (hex_color & 0xFF) / 255.0f;
    color.w = alpha;

    return color;
}

void ImAdd::DoubleText(ImVec4 color1, ImVec4 color2, const char* label1, const char* label2)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(std::string(label1 + std::string(label2)).c_str());
    const ImVec2 label1_size = CalcTextSize(label1, NULL, true);
    const ImVec2 label2_size = CalcTextSize(label2, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label1_size.x + label2_size.x, g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id)) {
        return;
    }

    window->DrawList->AddText(pos, GetColorU32(color1), label1);
    window->DrawList->AddText(pos + ImVec2(size.x - ImGui::CalcTextSize(label2).x, 0), GetColorU32(color2), label2);
}

void ImAdd::SeparatorText(const char* label, float thickness)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(-0.1f, g.FontSize), label_size.x, g.FontSize);

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb);
    if (!ItemAdd(total_bb, id)) {
        return;
    }

    window->DrawList->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), label);

    if (thickness > 0)
        window->DrawList->AddLine(pos + ImVec2(label_size.x + style.ItemInnerSpacing.x, size.y / 2), pos + ImVec2(size.x, size.y / 2), GetColorU32(ImGuiCol_Border), thickness);
}

void ImAdd::VSeparator(float margin, float thickness)
{
    if (thickness <= 0)
        return;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(ImVec2(thickness, -0.1f), thickness, thickness);

    const ImRect bb(pos, pos + size);
    const ImRect bb_rect(pos + ImVec2(0, margin), pos + size - ImVec2(0, margin));

    ItemSize(ImVec2(thickness, 0.0f));
    if (!ItemAdd(bb, 0))
        return;

    window->DrawList->AddRectFilled(bb_rect.Min, bb_rect.Max, GetColorU32(ImGuiCol_Border));
}

bool ImAdd::RadioFrame(const char* label, int* v, int radio_id, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held, active;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    active = *v == radio_id;
    if (pressed) {
        *v = radio_id;
    }

    // Colors
    ImVec4 colFrameMain = GetStyleColorVec4(ImGuiCol_Header);
    ImVec4 colFrameNull = colFrameMain; colFrameNull.w = 0.0f;
    ImVec4 colFrame = (active ? colFrameMain : colFrameNull);

    ImVec4 colBorderMain = GetStyleColorVec4(ImGuiCol_Border);
    ImVec4 colBorderNull = colBorderMain; colBorderNull.w = 0.0f;
    ImVec4 colBorder = (active ? colBorderMain : colBorderNull);

    ImVec4 colLabel = GetStyleColorVec4(active ? ImGuiCol_SliderGrab : (hovered && !held) ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    // Animations
    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Border;
        ImVec4 Label;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Label = colLabel;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Border = ImLerp(it_anim->second.Border, colBorder, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Label = ImLerp(it_anim->second.Label, colLabel, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    window->DrawList->AddRectFilled(pos, pos + size, GetColorU32(it_anim->second.Frame), style.FrameRounding);
    if (style.FrameBorderSize)
        window->DrawList->AddRect(pos, pos + size, GetColorU32(it_anim->second.Border), style.FrameRounding, 0, style.FrameBorderSize);

    window->DrawList->AddText(pos + ImVec2(size.x / 2 - label_size.x / 2, size.y / 2 - label_size.y / 2 + 1), GetColorU32(it_anim->second.Label), label);

    return pressed;
}

bool ImAdd::SelectableFrameIcon(const char* str_id, ImTextureID icon_texture, bool* active, const ImVec2& size_arg, const ImVec2& icon_padding)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    const ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, GetFrameHeight(), GetFrameHeight());

    const ImRect total_bb(pos, pos + size);
    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id))
    {
        IMGUI_TEST_ENGINE_ITEM_INFO(id, label, g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
        return false;
    }

    bool hovered, held;
    bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed)
    {
        *active = !*active;
        MarkItemEdited(id);
    }

    // Colors
    ImVec4 colFrame = GetStyleColorVec4(*active ? ImGuiCol_Header : ImGuiCol_ChildBg);
    ImVec4 colIcon = GetStyleColorVec4((*active || (hovered && !held)) ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    // Animation
    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Icon;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Icon = colIcon;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);
    it_anim->second.Icon = ImLerp(it_anim->second.Icon, colIcon, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);

    RenderNavHighlight(total_bb, id);

    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, GetColorU32(it_anim->second.Frame), style.TabRounding);
    if (style.FrameBorderSize > 0 && *active)
        window->DrawList->AddRect(total_bb.Min, total_bb.Max, GetColorU32(ImGuiCol_Border), style.TabRounding, 0, style.FrameBorderSize);

    if (icon_texture != ImTextureID_Invalid)
        window->DrawList->AddImage(icon_texture, pos + icon_padding, pos + size - icon_padding, ImVec2(), ImVec2(1, 1), GetColorU32(it_anim->second.Icon));

    return pressed;
}

bool ImAdd::RadioFrameGradient(const char* label, int* v, int radio_id, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held, active;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    active = *v == radio_id;
    if (pressed) {
        *v = radio_id;
    }

    // Colors
    ImVec4 colFrameMain = GetStyleColorVec4(ImGuiCol_SliderGrab); colFrameMain.w = 0.2f;
    ImVec4 colFrameNull = colFrameMain; colFrameNull.w = 0.0f;
    ImVec4 colFrame = (active ? colFrameMain : colFrameNull);

    ImVec4 colLabel = GetStyleColorVec4((active || (hovered && !held)) ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    ImVec4 colLine = GetStyleColorVec4(active ? ImGuiCol_SliderGrab : (hovered && !held) ? ImGuiCol_SliderGrabActive : ImGuiCol_Border);

    // Animations
    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Label;
        ImVec4 Line;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Label = colLabel;
        it_anim->second.Line = colLine;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Label = ImLerp(it_anim->second.Label, colLabel, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Line = ImLerp(it_anim->second.Line, colLine, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    ImVec4 transparent_col = it_anim->second.Frame; transparent_col.w = 0.0f;

    window->DrawList->AddRectFilledMultiColor(pos, pos + size, GetColorU32(transparent_col), GetColorU32(transparent_col), GetColorU32(it_anim->second.Frame), GetColorU32(it_anim->second.Frame));

    if (style.FrameBorderSize > 0) {
        window->DrawList->AddRectFilled(pos + ImVec2(0, size.y - style.FrameBorderSize), pos + size, GetColorU32(it_anim->second.Line));
    }

    window->DrawList->AddText(pos + ImVec2(style.FramePadding.x + style.FrameBorderSize * 2, size.y / 2 - label_size.y / 2 + 1), GetColorU32(it_anim->second.Label), label);

    return pressed;
}

bool ImAdd::RadioFrameText(const char* label, int* v, int radio_id, bool underline, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x, label_size.y);

    const ImRect bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held, active;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    active = *v == radio_id;
    if (pressed) {
        *v = radio_id;
    }

    // Colors
    ImVec4 colLabel = GetStyleColorVec4(active ? ImGuiCol_SliderGrab : (hovered && !held) ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    ImVec4 colUnderlineMain = GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImVec4 colUnderlineNull = colUnderlineMain; colUnderlineNull.w = 0.0f;
    ImVec4 colUnderline = (active ? colUnderlineMain : colUnderlineNull);

    // Animations
    struct stColors_State {
        ImVec4 Label;
        ImVec4 Underline;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Label = colLabel;
        it_anim->second.Underline = colUnderline;
    }

    it_anim->second.Label = ImLerp(it_anim->second.Label, colLabel, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.Underline = ImLerp(it_anim->second.Underline, colUnderline, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    const float text_x = pos.x + size.x * 0.5f - label_size.x * 0.5f;
    window->DrawList->AddText(ImVec2(text_x, pos.y + size.y * 0.5f - label_size.y * 0.5f + 1.f), GetColorU32(it_anim->second.Label), label);
    if (underline) {
        const float uy = pos.y + size.y - 1.f;
        window->DrawList->AddLine(ImVec2(text_x, uy), ImVec2(text_x + label_size.x, uy), ImGui::GetColorU32(it_anim->second.Underline), style.FrameBorderSize > 0 ? style.FrameBorderSize : 1.5f);
    }

    return pressed;
}

bool ImAdd::SelectableFrame(const char* str_id, int selected, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, g.FontSize, g.FontSize);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held, active;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Colors
    ImVec4 colFrameTop = GetStyleColorVec4(selected ? ImGuiCol_SliderGrab : ImGuiCol_Button);
    ImVec4 colFrameBottom = GetStyleColorVec4(selected ? ImGuiCol_SliderGrabActive : ImGuiCol_ButtonActive);

    // Animations
    struct stColors_State {
        ImVec4 FrameTop;
        ImVec4 FrameBottom;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.FrameTop = colFrameTop;
        it_anim->second.FrameBottom = colFrameBottom;
    }

    it_anim->second.FrameTop = ImLerp(it_anim->second.FrameTop, colFrameTop, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);
    it_anim->second.FrameBottom = ImLerp(it_anim->second.FrameBottom, colFrameBottom, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    window->DrawList->AddRectFilled(pos, pos + size, ImGui::GetColorU32(it_anim->second.FrameTop), style.FrameRounding);
    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(pos, pos + size, ImGui::GetColorU32(ImGuiCol_Border));
    }

    return pressed;
}

void ImAdd::BeginChild(const char* label, const ImVec2& size_arg)
{
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImGui::BeginChild(label, size_arg, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);

    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    ImDrawList* pDrawList = ImGui::GetWindowDrawList();

    pDrawList->AddRectFilled(pos + ImVec2(0, (float)(int)(label_size.y / 2)), pos + size, ImGui::GetColorU32(ImGuiCol_ChildBg), style.ChildRounding);
    pDrawList->AddRect(pos + ImVec2(0, (float)(int)(label_size.y / 2)), pos + size, ImGui::GetColorU32(ImGuiCol_Border), style.ChildRounding);
    pDrawList->AddRectFilled(pos + ImVec2(style.WindowPadding.x - style.ChildBorderSize * 5, 0), pos + ImVec2(style.WindowPadding.x + label_size.x + style.ChildBorderSize * 4, label_size.y), ImGui::GetColorU32(ImGuiCol_ChildBg));
    pDrawList->AddText(pos + ImVec2(style.WindowPadding.x, 0), ImGui::GetColorU32(ImGuiCol_Text), label);

    ImGui::SetCursorScreenPos(pos + ImVec2(0, (float)(int)(label_size.y / 2)));
    ImGui::BeginChild(std::string(std::string(label) + "##Main").c_str(), ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoBackground);
}

void ImAdd::EndChild()
{
    ImGui::EndChild();
    ImGui::EndChild();
}

bool ImAdd::Button(const char* label, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    // Colors
    ImVec4 colFrame = GetStyleColorVec4((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);

    // Animations
    struct stColors_State {
        ImVec4 Frame;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);
    
    window->DrawList->AddRectFilled(pos, pos + size, GetColorU32(it_anim->second.Frame), style.FrameRounding);
    if (style.FrameBorderSize > 0)
    {
        window->DrawList->AddRect(pos, pos + size, GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

    return pressed;
}

bool ImAdd::CheckBox(const char* label, bool* v)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    float height = g.FontSize;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(label_size.x > 0 ? label_size.x + style.ItemInnerSpacing.x + height : height, height);

    const ImRect bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        *v = !*v;

    // Colors
    ImVec4 colFrame = GetStyleColorVec4(*v ? ImGuiCol_SliderGrab : (hovered && !held) ? ImGuiCol_FrameBgHovered : held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);

    ImVec4 colBorderMain = GetStyleColorVec4(ImGuiCol_Border);
    ImVec4 colBorderNull = colBorderMain; colBorderNull.w = 0.0f;
    ImVec4 colBorder = (*v ? colBorderNull : colBorderMain);

    ImVec4 colCheckMarkMain = GetStyleColorVec4(ImGuiCol_CheckMark);
    ImVec4 colCheckMarkNull = colCheckMarkMain; colCheckMarkNull.w = 0.0f;
    ImVec4 colCheckMark = (*v ? colCheckMarkMain : colCheckMarkNull);

    // Animations
    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Border;
        ImVec4 CheckMark;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Border = colBorder;
        it_anim->second.CheckMark = colCheckMark;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);
    it_anim->second.Border = ImLerp(it_anim->second.Border, colBorder, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);
    it_anim->second.CheckMark = ImLerp(it_anim->second.CheckMark, colCheckMark, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    window->DrawList->AddRectFilled(pos, pos + ImVec2(height, size.y), GetColorU32(it_anim->second.Frame), style.FrameRounding);
    if (style.FrameBorderSize)
        window->DrawList->AddRect(pos, pos + ImVec2(height, size.y), GetColorU32(it_anim->second.Border), style.FrameRounding, 0, style.FrameBorderSize);

    float pad = 2.5;
    RenderCheckMark(window->DrawList, pos + ImVec2(pad, pad), GetColorU32(it_anim->second.CheckMark), height - pad * 2.0f);

    if (label_size.x > 0)
    {
        RenderText(pos + ImVec2(height + style.ItemInnerSpacing.x, 0), label);
    }

    return pressed;
}

bool ImAdd::ColorButton(const char* desc_id, const ImVec4& col, ImGuiColorEditFlags flags, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(desc_id);
    const float default_size = GetFrameHeight();
    const ImVec2 size(size_arg.x == 0.0f ? default_size : size_arg.x, size_arg.y == 0.0f ? default_size : size_arg.y);
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ItemSize(bb, (size.y >= default_size) ? g.Style.FramePadding.y : 0.0f);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    if (flags & ImGuiColorEditFlags_NoAlpha)
        flags &= ~(ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf);

    ImVec4 col_rgb = col;
    if (flags & ImGuiColorEditFlags_InputHSV)
        ColorConvertHSVtoRGB(col_rgb.x, col_rgb.y, col_rgb.z, col_rgb.x, col_rgb.y, col_rgb.z);

    ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);
    float grid_step = ImMin(size.x, size.y) / 2.99f;
    float rounding = ImMin(g.Style.FrameRounding, grid_step * 0.5f);
    ImRect bb_inner = bb;
    float off = 0.0f;
    if ((flags & ImGuiColorEditFlags_NoBorder) == 0)
    {
        off = -0.75f; // The border (using Col_FrameBg) tends to look off when color is near-opaque and rounding is enabled. This offset seemed like a good middle ground to reduce those artifacts.
        bb_inner.Expand(off);
    }
    if ((flags & ImGuiColorEditFlags_AlphaPreviewHalf) && col_rgb.w < 1.0f)
    {
        float mid_x = IM_ROUND((bb_inner.Min.x + bb_inner.Max.x) * 0.5f);
        RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(bb_inner.Min.x + grid_step, bb_inner.Min.y), bb_inner.Max, GetColorU32(col_rgb), grid_step, ImVec2(-grid_step + off, off), rounding, ImDrawFlags_RoundCornersRight);
        window->DrawList->AddRectFilled(bb_inner.Min, ImVec2(mid_x, bb_inner.Max.y), GetColorU32(col_rgb_without_alpha), rounding, ImDrawFlags_RoundCornersLeft);
    }
    else
    {
        // Because GetColorU32() multiplies by the global style Alpha and we don't want to display a checkerboard if the source code had no alpha
        ImVec4 col_source = (flags & ImGuiColorEditFlags_AlphaPreview) ? col_rgb : col_rgb_without_alpha;
        if (col_source.w < 1.0f)
            RenderColorRectWithAlphaCheckerboard(window->DrawList, bb_inner.Min, bb_inner.Max, GetColorU32(col_source), grid_step, ImVec2(off, off), rounding);
        else
            window->DrawList->AddRectFilled(bb_inner.Min, bb_inner.Max, GetColorU32(col_source), rounding);
    }
    RenderNavHighlight(bb, id);
    if ((flags & ImGuiColorEditFlags_NoBorder) == 0)
    {
        if (g.Style.FrameBorderSize > 0.0f)
        {
            RenderFrameBorder(bb.Min, bb.Max, rounding);
        }
        else
        {
            window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg), rounding); // Color button are often in need of some sort of border
        }
    }

    // Drag and Drop Source
    // NB: The ActiveId test is merely an optional micro-optimization, BeginDragDropSource() does the same test.
    if (g.ActiveId == id && !(flags & ImGuiColorEditFlags_NoDragDrop) && BeginDragDropSource())
    {
        if (flags & ImGuiColorEditFlags_NoAlpha)
            SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, &col_rgb, sizeof(float) * 3, ImGuiCond_Once);
        else
            SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, &col_rgb, sizeof(float) * 4, ImGuiCond_Once);
        ColorButton(desc_id, col, flags);
        SameLine();
        TextEx("Color");
        EndDragDropSource();
    }

    // Tooltip
    if (!(flags & ImGuiColorEditFlags_NoTooltip) && hovered && IsItemHovered(ImGuiHoveredFlags_ForTooltip))
        ColorTooltip(desc_id, &col.x, flags & (ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf));

    return pressed;
}

bool ImAdd::ColorEdit4(const char* label, float col[4])
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    (void)style;
    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    (void)label_size;
    const ImVec4 col_v4(col[0], col[1], col[2], col[3]);

    ImGuiColorEditFlags button_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoSidePreview;
    ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel;

    PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    bool result = ImAdd::ColorButton(label, col_v4, button_flags, ImVec2(g.FontSize * 2, g.FontSize));
    PopStyleVar();
    if (result)
    {
        OpenPopup(std::string(std::string(label) + "##ColorEdit4").c_str());
    }
    if (BeginPopup(std::string(std::string(label) + "##ColorEdit4").c_str()))
    {
        result |= ColorPicker4(label, col, picker_flags);
        EndPopup();
    }

    return result;
}

bool ImAdd::SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    char value_buf[64];
    DataTypeFormatString(value_buf, IM_ARRAYSIZE(value_buf), data_type, p_data, format ? format : DataTypeGetInfo(data_type)->PrintFmt);

    const ImVec2 label_size = CalcTextSize(label, NULL, true);
    float w = CalcItemWidth();
    ImVec2 pos = window->DC.CursorPos;

    // Compact track (smaller than full FrameHeight), label above.
    const float track_h = ImMax(style.GrabMinSize, g.FontSize * 0.55f);
    const float label_height = label_size.x > 0.0f ? (g.FontSize + style.ItemInnerSpacing.y) : 0.0f;
    const ImRect frame_bb(pos + ImVec2(0.0f, label_height), pos + ImVec2(w, label_height + track_h));
    const ImRect total_bb(pos, pos + ImVec2(w, label_height + track_h));

    ItemSize(total_bb, 0.0f);
    if (!ItemAdd(total_bb, id, &frame_bb))
        return false;

    if (format == NULL)
        format = DataTypeGetInfo(data_type)->PrintFmt;

    // IsMouseClicked(button, flags, owner) — do NOT pass widget id as flags (asserts in ImGui 1.92+).
    const bool hovered = ItemHoverable(frame_bb, id, g.LastItemData.ItemFlags);
    const bool clicked = hovered && IsMouseClicked(ImGuiMouseButton_Left);
    const bool make_active = (clicked || g.NavActivateId == id);

    if (make_active)
    {
        SetActiveID(id, window);
        SetFocusID(id, window);
        FocusWindow(window);
        g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
    }

    ImVec4 colFrame = GetStyleColorVec4(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImVec4 colGrab = GetStyleColorVec4(g.ActiveId == id ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);

    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Grab;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);
    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);
        it_anim->second.Frame = colFrame;
        it_anim->second.Grab = colGrab;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);
    it_anim->second.Grab = ImLerp(it_anim->second.Grab, colGrab, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);

    RenderNavHighlight(frame_bb, id);
    RenderFrame(frame_bb.Min, frame_bb.Max, GetColorU32(it_anim->second.Frame), true, g.Style.FrameRounding);

    ImRect grab_bb;
    const bool value_changed = SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, format, 0, &grab_bb);
    if (value_changed)
        MarkItemEdited(id);

    // Fill-style grab (left → thumb), full track height.
    if (grab_bb.Max.x > grab_bb.Min.x)
    {
        const ImVec2 fill_min = frame_bb.Min;
        const ImVec2 fill_max(grab_bb.Max.x, frame_bb.Max.y);
        window->DrawList->AddRectFilled(fill_min, fill_max, GetColorU32(it_anim->second.Grab), style.FrameRounding);
        if (style.FrameBorderSize > 0.0f)
            window->DrawList->AddRect(fill_min, fill_max, GetColorU32(ImGuiCol_SliderGrab), style.FrameRounding, 0, style.FrameBorderSize);
    }

    const float value_w = ImGui::CalcTextSize(value_buf).x;
    window->DrawList->AddText(pos + ImVec2(w - value_w, 0.0f), GetColorU32(ImGuiCol_TextDisabled), value_buf);
    if (label_size.x > 0.0f)
        RenderText(pos, label);

    return value_changed;
}

bool ImAdd::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format)
{
    return SliderScalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format);
}

bool ImAdd::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format)
{
    return SliderScalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format);
}

bool ImAdd::Selectable(const char* label, bool selected, const ImVec2& size_arg)
{
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    float borderSize = style.FrameBorderSize;

    // Colors
    ImVec4 colFrameMain = GetStyleColorVec4((hovered && !selected) ? held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered : ImGuiCol_Header);
    ImVec4 colFrameNull = colFrameMain; colFrameNull.w = 0.0f;
    ImVec4 colFrame = ((!hovered && !selected) ? colFrameNull : colFrameMain);

    ImVec4 colBorderMain = GetStyleColorVec4(ImGuiCol_Border);
    ImVec4 colBorderNull = colBorderMain; colBorderNull.w = 0.0f;
    ImVec4 colBorder = (selected ? colBorderMain : colBorderNull);

    // Animations
    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Border;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Border = colBorder;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);
    it_anim->second.Border = ImLerp(it_anim->second.Border, colBorder, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);

    // Render
    RenderNavHighlight(bb, id);

    window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(it_anim->second.Frame), style.FrameRounding);

    if (borderSize > 0)
        window->DrawList->AddRect(bb.Min, bb.Max, GetColorU32(it_anim->second.Border), style.FrameRounding, 0, borderSize);

    RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

    return pressed;
}

bool ImAdd::BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();

    ImGuiNextWindowDataFlags backup_next_window_data_flags = g.NextWindowData.HasFlags;
    g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
    if (window->SkipItems)
        return false;

    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    IM_ASSERT((flags & (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)) != (ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_NoPreview)); // Can't use both flags together
    if (flags & ImGuiComboFlags_WidthFitPreview)
        IM_ASSERT(((int)flags & ((int)ImGuiComboFlags_NoPreview | (int)ImGuiComboFlags_CustomPreview)) == 0);

    const float arrow_size = (flags & ImGuiComboFlags_NoArrowButton) ? 0.0f : GetFrameHeight();
    const ImVec2 label_size = CalcTextSize(label, NULL, true);

    const float preview_width = ((flags & ImGuiComboFlags_WidthFitPreview) && (preview_value != NULL)) ? CalcTextSize(preview_value, NULL, true).x : 0.0f;
    const float w = (flags & ImGuiComboFlags_NoPreview) ? arrow_size : ((flags & ImGuiComboFlags_WidthFitPreview) ? (arrow_size + preview_width + style.FramePadding.x * 2.0f) : CalcItemWidth());

    const ImRect bb(window->DC.CursorPos + ImVec2(0.0f, label_size.x > 0 ? label_size.y + style.ItemInnerSpacing.y : 0.0f), window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f) + ImVec2(0.0f, label_size.x > 0 ? label_size.y + style.ItemInnerSpacing.y : 0.0f));
    const ImRect total_bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, label_size.y + style.FramePadding.y * 2.0f) + ImVec2(0.0f, label_size.x > 0 ? label_size.y + style.ItemInnerSpacing.y : 0.0f));

    ItemSize(total_bb, style.FramePadding.y);
    if (!ItemAdd(total_bb, id, &bb))
        return false;

    // Open on click
    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    const ImGuiID popup_id = ImHashStr("##ComboPopup", 0, id);
    bool popup_open = IsPopupOpen(popup_id, ImGuiPopupFlags_None);
    if (pressed && !popup_open)
    {
        OpenPopupEx(popup_id, ImGuiPopupFlags_None);
        popup_open = true;
    }

    // Colors
    ImVec4 colFrame = GetStyleColorVec4(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImVec4 colText = GetStyleColorVec4((popup_open || hovered) ? ImGuiCol_Text : ImGuiCol_TextDisabled);

    // Animations
    struct stColors_State {
        ImVec4 Frame;
        ImVec4 Text;
    };

    static std::map<ImGuiID, stColors_State> anim;
    auto it_anim = anim.find(id);

    if (it_anim == anim.end())
    {
        anim.insert({ id, stColors_State() });
        it_anim = anim.find(id);

        it_anim->second.Frame = colFrame;
        it_anim->second.Text = colText;
    }

    it_anim->second.Frame = ImLerp(it_anim->second.Frame, colFrame, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);
    it_anim->second.Text = ImLerp(it_anim->second.Text, colText, 1.0f / IMADD_ANIMATIONS_SPEED * ImGui::GetIO().DeltaTime);

    // Render shape
    const float value_x2 = ImMax(bb.Min.x, bb.Max.x - arrow_size);
    RenderNavHighlight(bb, id);
    if (!(flags & ImGuiComboFlags_NoPreview)) {
        window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(it_anim->second.Frame), style.FrameRounding);
    }
    if (!(flags & ImGuiComboFlags_NoArrowButton))
    {
        if (value_x2 + arrow_size - style.FramePadding.x <= bb.Max.x)
        {
            RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y) + ImVec2(style.FrameBorderSize, -style.FrameBorderSize), GetColorU32(ImGuiCol_Border), ImGuiDir_Down, 1.0f);
            RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y) + ImVec2(-style.FrameBorderSize, -style.FrameBorderSize), GetColorU32(ImGuiCol_Border), ImGuiDir_Down, 1.0f);
            RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y) + ImVec2(0, style.FrameBorderSize * 2), GetColorU32(ImGuiCol_Border), ImGuiDir_Down, 1.0f);
            RenderArrow(window->DrawList, ImVec2(value_x2 + style.FramePadding.y, bb.Min.y + style.FramePadding.y), GetColorU32(it_anim->second.Text), ImGuiDir_Down, 1.0f);
        }
    }

    RenderFrameBorder(bb.Min, bb.Max, style.FrameRounding);

    // Custom preview
    if (flags & ImGuiComboFlags_CustomPreview)
    {
        g.ComboPreviewData.PreviewRect = ImRect(bb.Min.x, bb.Min.y, value_x2, bb.Max.y);
        IM_ASSERT(preview_value == NULL || preview_value[0] == 0);
        preview_value = NULL;
    }

    // Render preview and label
    if (preview_value != NULL && !(flags & ImGuiComboFlags_NoPreview))
    {
        if (g.LogEnabled)
            LogSetNextTextDecoration("{", "}");
        PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));
        RenderTextClipped(bb.Min + style.FramePadding, ImVec2(value_x2, bb.Max.y), preview_value, NULL, NULL);
        PopStyleColor();
    }
    if (label_size.x > 0)
        RenderText(total_bb.Min, label);

    if (!popup_open)
        return false;

    g.NextWindowData.HasFlags = backup_next_window_data_flags;
    return BeginComboPopup(popup_id, bb, flags);
}

static float CalcMaxPopupHeightFromItemCount(int items_count)
{
    ImGuiContext& g = *GImGui;
    if (items_count <= 0)
        return FLT_MAX;
    return (g.FontSize + g.Style.ItemSpacing.y) * items_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2);
}

bool ImAdd::BeginComboPopup(ImGuiID popup_id, const ImRect& bb, ImGuiComboFlags flags)
{
    ImGuiContext& g = *GImGui;
    if (!IsPopupOpen(popup_id, ImGuiPopupFlags_None))
    {
        g.NextWindowData.ClearFlags();
        return false;
    }

    // Set popup size
    float w = bb.GetWidth();
    if (g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)
    {
        g.NextWindowData.SizeConstraintRect.Min.x = ImMax(g.NextWindowData.SizeConstraintRect.Min.x, w);
    }
    else
    {
        if ((flags & ImGuiComboFlags_HeightMask_) == 0)
            flags |= ImGuiComboFlags_HeightRegular;
        IM_ASSERT(ImIsPowerOfTwo(flags & ImGuiComboFlags_HeightMask_)); // Only one
        int popup_max_height_in_items = -1;
        if (flags & ImGuiComboFlags_HeightRegular)     popup_max_height_in_items = 8;
        else if (flags & ImGuiComboFlags_HeightSmall)  popup_max_height_in_items = 4;
        else if (flags & ImGuiComboFlags_HeightLarge)  popup_max_height_in_items = 20;
        ImVec2 constraint_min(0.0f, 0.0f), constraint_max(FLT_MAX, FLT_MAX);
        if ((g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSize) == 0 || g.NextWindowData.SizeVal.x <= 0.0f) // Don't apply constraints if user specified a size
            constraint_min.x = w;
        if ((g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSize) == 0 || g.NextWindowData.SizeVal.y <= 0.0f)
            constraint_max.y = CalcMaxPopupHeightFromItemCount(popup_max_height_in_items);
        SetNextWindowSizeConstraints(constraint_min, constraint_max);
    }

    // Match stock ImGui 1.92+ combo window naming (BeginComboDepth, not BeginPopupStack.Size).
    // Using the wrong index + EndCombo() triggers: "Calling EndCombo() in wrong window!"
    char name[16];
    ImFormatString(name, IM_ARRAYSIZE(name), "##Combo_%02d", g.BeginComboDepth);

    if (ImGuiWindow* popup_window = FindWindowByName(name))
        if (popup_window->WasActive)
        {
            ImVec2 size_expected = CalcWindowNextAutoFitSize(popup_window);
            popup_window->AutoPosLastDirection = (flags & ImGuiComboFlags_PopupAlignLeft) ? ImGuiDir_Left : ImGuiDir_Down;
            ImRect r_outer = GetPopupAllowedExtentRect(popup_window);
            ImVec2 pos = FindBestWindowPosForPopupEx(bb.GetBL(), size_expected, &popup_window->AutoPosLastDirection, r_outer, bb, ImGuiPopupPositionPolicy_ComboBox);
            SetNextWindowPos(pos + ImVec2(0, g.Style.ItemSpacing.y));
        }

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
    PushStyleVar(ImGuiStyleVar_WindowPadding, g.Style.FramePadding);
    PushStyleVar(ImGuiStyleVar_PopupRounding, g.Style.FrameRounding);
    bool ret = Begin(name, NULL, window_flags);
    PopStyleVar(2);
    if (!ret)
    {
        EndPopup();
        return false;
    }
    g.BeginComboDepth++;
    return true;
}

bool ImAdd::Combo(const char* label, int* current_item, const char* (*getter)(void* user_data, int idx), void* user_data, int items_count, int popup_max_height_in_items)
{
    ImGuiContext& g = *GImGui;

    if (!current_item || items_count <= 0 || !getter)
        return false;

    if (*current_item < 0 || *current_item >= items_count)
        *current_item = 0;

    // Call the getter to obtain the preview string which is a parameter to BeginCombo()
    const char* preview_value = NULL;
    if (*current_item >= 0 && *current_item < items_count)
        preview_value = getter(user_data, *current_item);

    // The old Combo() API exposed "popup_max_height_in_items". The new more general BeginCombo() API doesn't have/need it, but we emulate it here.
    if (popup_max_height_in_items != -1 && !(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint))
        SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items)));

    if (!BeginCombo(label, preview_value, ImGuiComboFlags_None))
        return false;

    // Display items
    // FIXME-OPT: Use clipper (but we need to disable it on the appearing frame to make sure our call to SetItemDefaultFocus() is processed)
    bool value_changed = false;
    for (int i = 0; i < items_count; i++)
    {
        const char* item_text = getter(user_data, i);
        if (item_text == NULL)
            item_text = "*Unknown item*";

        PushID(i);
        const bool item_selected = (i == *current_item);
        PushStyleVar(ImGuiStyleVar_ItemSpacing, g.Style.FramePadding);
        if (Selectable(item_text, item_selected, ImVec2(-0.1f, 0)) && *current_item != i)
        {
            value_changed = true;
            *current_item = i;
            //CloseCurrentPopup();
        }
        PopStyleVar();
        if (item_selected)
            SetItemDefaultFocus();
        PopID();
    }

    // Must pair with BeginComboPopup's BeginComboDepth++ (stock EndCombo does this).
    EndCombo();

    if (value_changed)
        MarkItemEdited(g.LastItemData.ID);

    return value_changed;
}

// Getter for the old Combo() API: const char*[]
static const char* Items_ArrayGetter(void* data, int idx)
{
    const char* const* items = (const char* const*)data;
    return items[idx];
}

// Getter for the old Combo() API: "item1\0item2\0item3\0"
static const char* Items_SingleStringGetter(void* data, int idx)
{
    const char* items_separated_by_zeros = (const char*)data;
    int items_count = 0;
    const char* p = items_separated_by_zeros;
    while (*p)
    {
        if (idx == items_count)
            break;
        p += strlen(p) + 1;
        items_count++;
    }
    return *p ? p : NULL;
}

bool ImAdd::Combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items)
{
    const bool value_changed = Combo(label, current_item, Items_ArrayGetter, (void*)items, items_count, height_in_items);
    return value_changed;
}

bool ImAdd::Combo(const char* label, int* current_item, const char* items_separated_by_zeros, float width, int height_in_items)
{
    int items_count = 0;
    const char* p = items_separated_by_zeros;       // FIXME-OPT: Avoid computing this, or at least only when combo is open
    while (*p)
    {
        p += strlen(p) + 1;
        items_count++;
    }

    const float w = CalcItemSize(ImVec2(width, 0), CalcItemWidth(), 0).x;
    PushItemWidth(w);
    bool value_changed = Combo(label, current_item, Items_SingleStringGetter, (void*)items_separated_by_zeros, items_count, height_in_items);
    PopItemWidth();

    return value_changed;
}

namespace {
const char* KeyNameSafe( int vk )
{
    if ( vk <= 0 || vk >= (int)IM_ARRAYSIZE( szKeyNames ) )
        return "None";
    return szKeyNames[vk];
}

// Capture via GetAsyncKeyState — works with our overlay hooks (io.KeysDown is legacy/unreliable).
int PollBindKey( bool ignore_lmb )
{
    static const int k_mouse[] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    for ( int vk : k_mouse )
    {
        if ( ignore_lmb && vk == VK_LBUTTON )
            continue;
        if ( GetAsyncKeyState( vk ) & 0x8000 )
            return vk;
    }
    for ( int vk = 0x08; vk <= 0xFE; ++vk )
    {
        if ( vk == VK_ESCAPE )
            continue;
        if ( GetAsyncKeyState( vk ) & 0x8000 )
            return vk;
    }
    return 0;
}
} // namespace

bool ImAdd::KeyBind(const char* str_id, int* k, float custom_width)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiIO& io = g.IO;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(str_id);

    if ( !k )
        return false;
    if ( *k < 0 || *k >= (int)IM_ARRAYSIZE( szKeyNames ) )
        *k = 0;

    char buf_display[32] = "None";
    ImVec2 pos = window->DC.CursorPos;
    float width = custom_width == 0 ? ImGui::CalcItemSize(ImVec2(-0.1f, 0), 0, 0).x : custom_width;
    float height = ImGui::GetFontSize();

    ImVec2 size = ImVec2(width, height);
    ImRect frame_bb(pos, pos + size);
    ImRect total_bb(pos, frame_bb.Max);

    ImGui::ItemSize(total_bb);
    if (!ImGui::ItemAdd(total_bb, id))
        return false;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0);
    if (hovered)
    {
        ImGui::SetHoveredID(id);
        g.MouseCursor = ImGuiMouseCursor_Hand;
    }

    // Enter bind mode on LMB — do NOT clear existing key (that caused "half second" dead aim).
    if (hovered && io.MouseClicked[0] && g.ActiveId != id)
    {
        ImGui::SetActiveID(id, window);
        ImGui::FocusWindow(window);
    }

    bool value_changed = false;

    if (g.ActiveId == id)
    {
        // Skip the click that activated us (LMB still down).
        const bool activator_held = ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 ) != 0;
        if ( !activator_held )
        {
            if ( ImGui::IsKeyPressed( ImGuiKey_Escape ) )
            {
                ImGui::ClearActiveID();
            }
            else
            {
                const int polled = PollBindKey( /*ignore_lmb=*/false );
                if ( polled != 0 )
                {
                    *k = polled;
                    value_changed = true;
                    ImGui::ClearActiveID();
                }
            }
        }
    }

    ImGui::RenderNavHighlight(total_bb, id);

    if (g.ActiveId == id)
        strcpy_s(buf_display, sizeof buf_display, "...");
    else if (*k != 0)
        strcpy_s(buf_display, sizeof buf_display, KeyNameSafe(*k));

    const ImRect clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + size.x, frame_bb.Min.y + size.y);
    ImGui::RenderTextClipped(frame_bb.Min, frame_bb.Max, buf_display, NULL, NULL, style.ButtonTextAlign, &clip_rect);

    return value_changed;
}

void ImAdd::KeyBindPopup(const char* str_id, int* k, int* mode)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    if ( !k )
        return;
    if ( *k < 0 || *k >= (int)IM_ARRAYSIZE( szKeyNames ) )
        *k = 0;

    const float key_w = ImGui::CalcTextSize( KeyNameSafe( *k ) ).x;
    ImAdd::KeyBind(str_id, k, key_w > 1.f ? key_w : ImGui::CalcTextSize( "None" ).x );

    char popup_name[128];
    std::snprintf( popup_name, sizeof( popup_name ), "%s##kb_popup", str_id );

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right, false))
        ImGui::OpenPopup(popup_name);

    if (ImGui::BeginPopup(popup_name))
    {
        // Matches Lefrizzel KeyMode: 0=Always, 1=Hold, 2=Toggle
        ImGui::BeginChild("PopupRect",
            ImVec2(ImGui::CalcTextSize("Always").x * 3.5f + style.ItemSpacing.x * 2, ImGui::GetFontSize()),
            ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
        {
            ImAdd::RadioFrameText("Always", mode, 0, false);
            ImGui::SameLine(); ImAdd::VSeparator(style.WindowPadding.y, 1.0f); ImGui::SameLine();
            ImAdd::RadioFrameText("Hold", mode, 1, false);
            ImGui::SameLine(); ImAdd::VSeparator(style.WindowPadding.y, 1.0f); ImGui::SameLine();
            ImAdd::RadioFrameText("Toggle", mode, 2, false);
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}