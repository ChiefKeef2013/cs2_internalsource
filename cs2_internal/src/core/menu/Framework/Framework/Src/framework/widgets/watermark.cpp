#include <core/common.hpp>
#include <core/settings.hpp>
#include "../headers/functions.h"
#include "../headers/widgets.h"
#include <modules/economy/economy.h>
#include <modules/economy/cloud_config.h>
#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBasePlayerController.h>
#include <algorithm>
#include <array>
#include <d3dcompiler.h>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace {

const char* blur_shader_hlsl = R"(
cbuffer BlurBuffer : register(b0)
{
    float2 resolution;
    float blurAmount;
    float padding;
};

Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

struct VS_INPUT
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target
{
    float2 texelSize = 1.0 / resolution * blurAmount;
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    float weights[9] = {
        0.0625, 0.125, 0.0625,
        0.125,  0.25,  0.125,
        0.0625, 0.125, 0.0625
    };

    int index = 0;
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            color += tex.Sample(samplerState, input.uv + offset) * weights[index++];
        }
    }

    return color;
}
)";

}

void c_watermark::initialize(ID3D11Device* device)
{
    if (initialized)
        return;
    
    try
    {
        create_blur_shader(device);
        initialized = true;
    }
    catch (...)
    {
        initialized = false;
    }
}

void c_watermark::start_timer()
{
    if (!timer_started)
    {
        injection_time = std::chrono::steady_clock::now();
        timer_started = true;
    }
}

void c_watermark::create_blur_shader(ID3D11Device* device)
{
    if (!device)
        return;
    
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* error_blob = nullptr;
    
    HRESULT hr = D3DCompile(
        blur_shader_hlsl,
        strlen(blur_shader_hlsl),
        nullptr,
        nullptr,
        nullptr,
        "VS",
        "vs_5_0",
        0,
        0,
        &vs_blob,
        &error_blob
    );
    
    if (SUCCEEDED(hr))
    {
        device->CreateVertexShader(
            vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(),
            nullptr,
            &vertex_shader
        );
    }
    
    if (error_blob) error_blob->Release();
    error_blob = nullptr;
    
    hr = D3DCompile(
        blur_shader_hlsl,
        strlen(blur_shader_hlsl),
        nullptr,
        nullptr,
        nullptr,
        "PS",
        "ps_5_0",
        0,
        0,
        &ps_blob,
        &error_blob
    );
    
    if (SUCCEEDED(hr))
    {
        device->CreatePixelShader(
            ps_blob->GetBufferPointer(),
            ps_blob->GetBufferSize(),
            nullptr,
            &pixel_shader
        );
    }
    
    if (vs_blob) vs_blob->Release();
    if (ps_blob) ps_blob->Release();
    if (error_blob) error_blob->Release();
    
    D3D11_BUFFER_DESC cb_desc = {};
    cb_desc.ByteWidth = sizeof(float) * 4; 
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    device->CreateBuffer(&cb_desc, nullptr, &constant_buffer);
    
    D3D11_SAMPLER_DESC sampler_desc = {};
    sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    
    device->CreateSamplerState(&sampler_desc, &sampler_state);
    
    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    device->CreateBlendState(&blend_desc, &blend_state);
}

void c_watermark::render()
{
    if (!gui || !draw || !font)
        return;

    economy::g_cloud.consume_pending_load( );
    
    float spawn_alpha = var->gui.watermark_spawn_alpha;
    if (spawn_alpha <= 0.01f)
        return;
    
    
    float spawn_scale = 0.85f + (spawn_alpha * 0.15f);
    float scale = spawn_scale;
    
    
    if (!timer_started)
        start_timer();
    
    try
    {
        enum item_id : int
        {
            item_user,
            item_fps,
            item_ping,
            item_time,
            item_count
        };

        struct item_entry
        {
            int id{};
            const char* text{};
            ImVec2 size{};
            ImRect target_rect{};
        };

        auto& cfg = settings::g_misc.m_watermark;
        auto order_for = [&](int id) -> config::val<int>&
        {
            switch (id)
            {
            case item_user: return cfg.user_order;
            case item_fps: return cfg.fps_order;
            case item_ping: return cfg.ping_order;
            default: return cfg.time_order;
            }
        };

        std::array<int, item_count> order{ item_user, item_fps, item_ping, item_time };
        std::sort(order.begin(), order.end(), [&](int a, int b)
        {
            const int lhs = std::clamp(order_for(a).value, 0, item_count - 1);
            const int rhs = std::clamp(order_for(b).value, 0, item_count - 1);
            return lhs == rhs ? a < b : lhs < rhs;
        });
        for (int i = 0; i < item_count; ++i)
            order_for(order[i]).value = i;

        ImGuiIO& io = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetForegroundDrawList();

        const char* brand_text = "cs2_internal";
        static std::string user_buf;
        {
            const std::string name = economy::g_tracker.get_display_name();
            user_buf = name.empty() ? std::string("user") : name;
        }
        const char* user_text = user_buf.c_str();

        char fps_text[32]{};
        std::snprintf(fps_text, sizeof(fps_text), "FPS: %d", static_cast<int>(io.Framerate));

        char ping_text[32]{};
        int ping = 0;
        const auto local = systems::g_local.get();
        if (local.controller && memory::is_game_ptr(local.controller))
            ping = static_cast<int>(reinterpret_cast<CCSPlayerController*>(local.controller)->m_iPing());
        std::snprintf(ping_text, sizeof(ping_text), "Ping: %dms", ping);

        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - injection_time);
        const int hours = static_cast<int>(elapsed.count() / 3600);
        const int minutes = static_cast<int>((elapsed.count() % 3600) / 60);
        const int seconds = static_cast<int>(elapsed.count() % 60);

        char timer_text[64]{};
        std::snprintf(timer_text, sizeof(timer_text), "Time: %02d:%02d:%02d", hours, minutes, seconds);

        ImFont* text_font = font->get(main_font_data, 13.0f);
        if (!text_font)
            return;

        auto is_visible = [&](int id)
        {
            switch (id)
            {
            case item_user: return cfg.show_user.value;
            case item_fps: return cfg.show_fps.value;
            case item_ping: return cfg.show_ping.value;
            case item_time: return cfg.show_time.value;
            default: return false;
            }
        };

        auto text_for = [&](int id) -> const char*
        {
            switch (id)
            {
            case item_user: return user_text;
            case item_fps: return fps_text;
            case item_ping: return ping_text;
            case item_time: return timer_text;
            default: return "";
            }
        };

        std::vector<item_entry> items{};
        items.reserve(item_count);
        for (const int id : order)
        {
            if (!is_visible(id))
                continue;
            const char* text = text_for(id);
            items.push_back({ id, text, text_font->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, text), {} });
        }

        const ImVec2 brand_size = text_font->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, brand_text);
        const float padding = SCALE(15.0f);
        const float separator_width = SCALE(8.0f);
        float total_width = brand_size.x + padding * 2.0f;
        for (const auto& item : items)
            total_width += separator_width + item.size.x;

        const float box_height = SCALE(32.0f);
        ImVec2 pos(watermark_pos.x, watermark_pos.y);
        ImVec2 size(total_width, box_height);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        ImVec2 scaled_pos(
            center.x + (pos.x - center.x) * spawn_scale,
            center.y + (pos.y - center.y) * spawn_scale);
        ImVec2 scaled_size(size.x * spawn_scale, size.y * spawn_scale);
        ImRect watermark_rect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y));

        const bool menu_open = var->gui.menu_open;
        const ImVec2 mouse_pos = ImGui::GetMousePos();

        float text_y = scaled_pos.y + (scaled_size.y - brand_size.y * scale) * 0.5f;
        float cursor_x = scaled_pos.x + padding * scale + brand_size.x * scale;
        for (auto& item : items)
        {
            cursor_x += separator_width * 0.5f * scale;
            cursor_x += separator_width * 0.5f * scale;
            item.target_rect = ImRect(
                ImVec2(cursor_x, text_y),
                ImVec2(cursor_x + item.size.x * scale, text_y + item.size.y * scale));
            cursor_x += item.size.x * scale;
        }

        if (menu_open && watermark_rect.Contains(mouse_pos) && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            context_open = true;
            context_pos = mouse_pos;
        }

        int hovered_item = -1;
        for (const auto& item : items)
        {
            const int id = item.id;
            const float visual_x = item_offsets_seeded[id] ? item_offsets[id] : item.target_rect.Min.x;
            ImRect visual_rect(
                ImVec2(visual_x, item.target_rect.Min.y),
                ImVec2(visual_x + item.target_rect.GetWidth(), item.target_rect.Max.y));
            if (visual_rect.Contains(mouse_pos))
            {
                hovered_item = id;
                break;
            }
        }

        if (menu_open && hovered_item != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            dragging_item = hovered_item;
            float hovered_x = item_offsets[hovered_item];
            for (const auto& item : items)
            {
                if (item.id == hovered_item)
                {
                    hovered_x = item_offsets_seeded[hovered_item] ? item_offsets[hovered_item] : item.target_rect.Min.x;
                    break;
                }
            }
            item_drag_offset = ImVec2(mouse_pos.x - hovered_x, mouse_pos.y - text_y);
            is_dragging = false;
        }
        else if (menu_open && watermark_rect.Contains(mouse_pos) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            is_dragging = true;
            drag_offset = ImVec2(mouse_pos.x - pos.x, mouse_pos.y - pos.y);
        }

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            is_dragging = false;
            dragging_item = -1;
        }

        if (menu_open && dragging_item != -1 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            float dragged_width = 0.0f;
            for (const auto& item : items)
            {
                if (item.id == dragging_item)
                {
                    dragged_width = item.target_rect.GetWidth();
                    break;
                }
            }

            const float dragged_center = mouse_pos.x - item_drag_offset.x + dragged_width * 0.5f;
            for (const auto& item : items)
            {
                if (item.id == dragging_item)
                    continue;
                if (dragged_center >= item.target_rect.Min.x && dragged_center <= item.target_rect.Max.x)
                    std::swap(order_for(dragging_item).value, order_for(item.id).value);
            }
        }

        if (menu_open && is_dragging)
        {
            watermark_pos.x = mouse_pos.x - drag_offset.x;
            watermark_pos.y = mouse_pos.y - drag_offset.y;
            watermark_pos.x = ImClamp(watermark_pos.x, 0.0f, io.DisplaySize.x - total_width);
            watermark_pos.y = ImClamp(watermark_pos.y, 0.0f, io.DisplaySize.y - box_height);

            pos = ImVec2(watermark_pos.x, watermark_pos.y);
            center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
            scaled_pos = ImVec2(
                center.x + (pos.x - center.x) * spawn_scale,
                center.y + (pos.y - center.y) * spawn_scale);
            scaled_size = ImVec2(size.x * spawn_scale, size.y * spawn_scale);
            watermark_rect = ImRect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y));
        }

        const float shadow_blur = SCALE(8.0f);
        for (int s = 0; s < 4; s++)
        {
            const float progress = static_cast<float>(s) / 4.0f;
            const float alpha = (1.0f - progress) * 0.15f * spawn_alpha;
            const float blur_amount = shadow_blur * progress;

            draw_list->AddRectFilled(
                ImVec2(scaled_pos.x - blur_amount, scaled_pos.y - blur_amount),
                ImVec2(scaled_pos.x + scaled_size.x + blur_amount, scaled_pos.y + scaled_size.y + blur_amount),
                IM_COL32(0, 0, 0, static_cast<int>(alpha * 255)),
                SCALE(6.0f) + blur_amount);
        }

        draw_list->AddRectFilled(
            scaled_pos,
            ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
            draw->get_clr(ImVec4(30.0f / 255.0f, 30.0f / 255.0f, 35.0f / 255.0f, 0.95f * spawn_alpha)),
            SCALE(6.0f));

        draw_list->AddRect(
            scaled_pos,
            ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y),
            draw->get_clr(ImVec4(0.0f, 0.0f, 0.0f, 0.8f * spawn_alpha)),
            SCALE(6.0f),
            0,
            SCALE(1.5f));

        const ImVec4 text_white(0.95f, 0.95f, 0.95f, spawn_alpha);
        const ImVec4 text_gray(0.7f, 0.7f, 0.75f, spawn_alpha);
        const ImVec4 text_green(0.3f, 0.95f, 0.3f, spawn_alpha);
        const ImVec4 text_purple(179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, spawn_alpha);
        const ImVec4 separator_color(0.4f, 0.4f, 0.45f, 0.5f * spawn_alpha);

        text_y = scaled_pos.y + (scaled_size.y - brand_size.y * scale) * 0.5f;
        cursor_x = scaled_pos.x + padding * scale;

        draw_list->AddText(text_font, 13.0f, ImVec2(cursor_x, text_y), draw->get_clr(text_purple), brand_text);
        cursor_x += brand_size.x * scale;

        for (const auto& item : items)
        {
            cursor_x += separator_width * 0.5f * scale;
            draw_list->AddLine(
                ImVec2(cursor_x, scaled_pos.y + SCALE(8.0f) * scale),
                ImVec2(cursor_x, scaled_pos.y + scaled_size.y - SCALE(8.0f) * scale),
                draw->get_clr(separator_color),
                SCALE(1.0f));
            cursor_x += separator_width * 0.5f * scale;

            const int id = item.id;
            const float target_x = cursor_x;
            if (!item_offsets_seeded[id])
            {
                item_offsets[id] = target_x;
                item_offsets_seeded[id] = true;
            }

            if (dragging_item == id && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                item_offsets[id] = mouse_pos.x - item_drag_offset.x;
            else
                gui->easing(item_offsets[id], target_x, menu_motion::k_control_active, dynamic_easing);

            ImVec4 color = text_white;
            if (id == item_user)
                color = text_gray;
            else if (id == item_fps)
                color = text_green;

            draw_list->AddText(text_font, 13.0f, ImVec2(item_offsets[id], text_y), draw->get_clr(color), item.text);
            cursor_x += item.size.x * scale;
        }

        gui->easing(context_alpha, context_open ? 1.0f : 0.0f, menu_motion::k_control_active, dynamic_easing);
        if (context_alpha > 0.01f)
        {
            ImGui::SetNextWindowPos(context_pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(SCALE(168.0f), 0.0f), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SCALE(8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(SCALE(10.0f), SCALE(8.0f)));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(SCALE(8.0f), SCALE(menu_theme::k_compact_spacing)));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.15f, 0.95f * context_alpha));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.28f, 0.32f, 0.80f * context_alpha));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.92f, context_alpha));

            const ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::Begin("##watermark_context", nullptr, flags))
            {
                auto toggle_item = [](std::string_view label, xui::setting& setting)
                {
                    widgets->checkbox(label, &setting.value);
                    setting.bind.active = setting.value;
                };

                toggle_item("user", cfg.show_user);
                toggle_item("fps", cfg.show_fps);
                toggle_item("ping", cfg.show_ping);
                toggle_item("time", cfg.show_time);

                const bool popup_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                if (context_open && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !popup_hovered && !watermark_rect.Contains(mouse_pos))
                    context_open = false;
                if (context_open && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !popup_hovered && !watermark_rect.Contains(mouse_pos))
                    context_open = false;
            }
            ImGui::End();

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
        }
    }
    catch (...)
    {
        is_dragging = false;
        dragging_item = -1;
    }
}

void c_watermark::cleanup()
{
    if (vertex_shader) { vertex_shader->Release(); vertex_shader = nullptr; }
    if (pixel_shader) { pixel_shader->Release(); pixel_shader = nullptr; }
    if (constant_buffer) { constant_buffer->Release(); constant_buffer = nullptr; }
    if (sampler_state) { sampler_state->Release(); sampler_state = nullptr; }
    if (blend_state) { blend_state->Release(); blend_state = nullptr; }
    
    initialized = false;
}

