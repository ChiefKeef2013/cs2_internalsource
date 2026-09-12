#include "../headers/functions.h"
#include "../headers/widgets.h"
#include "../data/images.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>

#pragma warning(disable: 4996)
#pragma comment(lib, "WindowsApp.lib")

#include <stb/stb_image.h>

struct ImGui_ImplDX11_Data
{
    ID3D11Device*            pd3dDevice;
    ID3D11DeviceContext*     pd3dDeviceContext;
    
};

static ImGui_ImplDX11_Data* GetDX11Data()
{
    return (ImGui_ImplDX11_Data*)ImGui::GetIO().BackendRendererUserData;
}

static const char* ambient_vertex_shader = R"(
struct VSOut {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOut main(uint id : SV_VertexID) {
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOut o;
    o.pos = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    o.uv = float2(uv.x, 1.0 - uv.y);
    return o;
}
)";

static const char* ambient_pixel_shader = R"(
cbuffer Constants : register(b0) {
    float4 params[8];
    // params[0].xy = card_size (pixels)
    // params[1].xy = center1, params[1].z = radius1
    // params[2] = color1 (rgba, a=strength)
    // params[3].xy = center2, params[3].z = radius2
    // params[4] = color2
    // params[5].xy = center3, params[5].z = radius3
    // params[6] = color3
};

struct PSIn {
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

float blob(float2 px, float2 center, float radius, float strength) {
    float d = length(px - center) / max(radius, 1.0);
    return saturate(1.0 - d * d) * strength;
}

float4 main(PSIn input) : SV_Target {
    float2 card_size = params[0].xy;
    float2 px = input.uv * card_size;
    
    // Base dark color
    float3 col = float3(12.0/255.0, 12.0/255.0, 16.0/255.0);
    
    // Add color blobs
    col += params[2].rgb * blob(px, params[1].xy, params[1].z, params[2].a);
    col += params[4].rgb * blob(px, params[3].xy, params[3].z, params[4].a);
    col += params[6].rgb * blob(px, params[5].xy, params[5].z, params[6].a);
    
    return float4(saturate(col), 1.0);
}
)";

void c_ambient_background::init(ID3D11Device* device)
{
    if (initialized || !device)
        return;
        
    
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = TEX_WIDTH;
    tex_desc.Height = TEX_HEIGHT;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    
    if (FAILED(device->CreateTexture2D(&tex_desc, nullptr, &render_texture)))
        return;
        
    device->CreateRenderTargetView(render_texture, nullptr, &render_target);
    device->CreateShaderResourceView(render_texture, nullptr, &shader_resource);
    
    
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* error = nullptr;
    
    if (SUCCEEDED(D3DCompile(ambient_vertex_shader, strlen(ambient_vertex_shader), nullptr, nullptr, nullptr, 
        "main", "vs_5_0", 0, 0, &vs_blob, &error)))
    {
        device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vertex_shader);
        vs_blob->Release();
    }
    if (error) { error->Release(); error = nullptr; }
    
    if (SUCCEEDED(D3DCompile(ambient_pixel_shader, strlen(ambient_pixel_shader), nullptr, nullptr, nullptr, 
        "main", "ps_5_0", 0, 0, &ps_blob, &error)))
    {
        device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &pixel_shader);
        ps_blob->Release();
    }
    if (error) error->Release();
    
    
    D3D11_BUFFER_DESC cb_desc = {};
    cb_desc.ByteWidth = 128;
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cb_desc, nullptr, &constant_buffer);
    
    
    D3D11_RASTERIZER_DESC rast_desc = {};
    rast_desc.FillMode = D3D11_FILL_SOLID;
    rast_desc.CullMode = D3D11_CULL_NONE;
    device->CreateRasterizerState(&rast_desc, &rasterizer_state);
    
    
    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].BlendEnable = FALSE;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend_desc, &blend_state);
    
    initialized = (vertex_shader && pixel_shader && constant_buffer && render_target && shader_resource);
}

void c_ambient_background::render(ID3D11DeviceContext* context, ImVec2 pos, ImVec2 size, float dominant_colors[3][3])
{
    if (!initialized || !context)
        return;
        
    
    ID3D11RenderTargetView* prev_rtv = nullptr;
    ID3D11DepthStencilView* prev_dsv = nullptr;
    context->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);
    
    D3D11_VIEWPORT prev_vp = {};
    UINT num_vp = 1;
    context->RSGetViewports(&num_vp, &prev_vp);
    
    ID3D11BlendState* prev_bs = nullptr;
    float prev_bf[4] = {};
    UINT prev_sm = 0;
    context->OMGetBlendState(&prev_bs, prev_bf, &prev_sm);
    
    
    context->OMSetRenderTargets(1, &render_target, nullptr);
    
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)TEX_WIDTH;
    vp.Height = (float)TEX_HEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
    context->RSSetState(rasterizer_state);
    context->OMSetBlendState(blend_state, nullptr, 0xffffffff);
    
    
    float time = (float)ImGui::GetTime();
    float cw = size.x;
    float ch = size.y;
    
    
    float cx1 = cw * 0.30f + std::sin(time * 0.25f) * cw * 0.15f;
    float cy1 = ch * 0.20f + std::cos(time * 0.35f) * ch * 0.10f;
    
    float cx2 = cw * 0.70f + std::cos(time * 0.20f) * cw * 0.15f;
    float cy2 = ch * 0.50f + std::sin(time * 0.30f) * ch * 0.10f;
    
    float cx3 = cw * 0.50f + std::cos(time * 0.35f) * cw * 0.10f;
    float cy3 = ch * 0.80f + std::sin(time * 0.18f) * ch * 0.10f;
    
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        float* data = (float*)mapped.pData;
        
        
        data[0] = cw; data[1] = ch; data[2] = 0; data[3] = 0;
        
        
        data[4] = cx1; data[5] = cy1; data[6] = 200.0f; data[7] = 0;
        
        
        data[8] = dominant_colors[0][0]; 
        data[9] = dominant_colors[0][1]; 
        data[10] = dominant_colors[0][2]; 
        data[11] = 0.30f; 
        
        
        data[12] = cx2; data[13] = cy2; data[14] = 220.0f; data[15] = 0;
        
        
        data[16] = dominant_colors[1][0];
        data[17] = dominant_colors[1][1];
        data[18] = dominant_colors[1][2];
        data[19] = 0.25f;
        
        
        data[20] = cx3; data[21] = cy3; data[22] = 180.0f; data[23] = 0;
        
        
        data[24] = dominant_colors[2][0];
        data[25] = dominant_colors[2][1];
        data[26] = dominant_colors[2][2];
        data[27] = 0.22f;
        
        context->Unmap(constant_buffer, 0);
    }
    
    
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetInputLayout(nullptr);
    context->VSSetShader(vertex_shader, nullptr, 0);
    context->PSSetShader(pixel_shader, nullptr, 0);
    context->PSSetConstantBuffers(0, 1, &constant_buffer);
    context->Draw(3, 0);
    
    
    context->OMSetRenderTargets(1, &prev_rtv, prev_dsv);
    context->RSSetViewports(1, &prev_vp);
    context->OMSetBlendState(prev_bs, prev_bf, prev_sm);
    
    if (prev_rtv) prev_rtv->Release();
    if (prev_dsv) prev_dsv->Release();
    if (prev_bs) prev_bs->Release();
}

void c_ambient_background::cleanup()
{
    if (blend_state) { blend_state->Release(); blend_state = nullptr; }
    if (rasterizer_state) { rasterizer_state->Release(); rasterizer_state = nullptr; }
    if (constant_buffer) { constant_buffer->Release(); constant_buffer = nullptr; }
    if (pixel_shader) { pixel_shader->Release(); pixel_shader = nullptr; }
    if (vertex_shader) { vertex_shader->Release(); vertex_shader = nullptr; }
    if (shader_resource) { shader_resource->Release(); shader_resource = nullptr; }
    if (render_target) { render_target->Release(); render_target = nullptr; }
    if (render_texture) { render_texture->Release(); render_texture = nullptr; }
    initialized = false;
}

void c_musicplayer::setup()
{
    if (initialized)
        return;
        
    
    lyrics_available = false;
    lyrics_synced = false;
    lyrics_fetching = false;
    current_lyric_line = -1;
    last_lyrics_track = "";
    lyrics_scroll_y = 0.0f;
    lyrics_scroll_target = 0.0f;
    synced_lyrics.clear();
    plain_lyrics.clear();
    lyrics_height = SCALE(110.0f);
    lyrics_expanded = false;
        
    try
    {
        winrt::init_apartment();
        session_manager = winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        initialized = true;
    }
    catch (...)
    {
        initialized = false;
    }
}

void c_musicplayer::shutdown()
{
    if (thumbnail_buffer)
    {
        free(thumbnail_buffer);
        thumbnail_buffer = nullptr;
    }
    
    if (thumbnail_texture)
    {
        thumbnail_texture->Release();
        thumbnail_texture = nullptr;
    }
    
    
    ambient_bg.cleanup();
    
    session = nullptr;
    session_manager = nullptr;
    
    if (initialized)
    {
        winrt::uninit_apartment();
        initialized = false;
    }
}

std::string c_musicplayer::format_time(uint64_t time_100ns)
{
    if (time_100ns == 0)
        return "0:00";
        
    std::chrono::system_clock::duration duration(time_100ns);
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();
    
    if (minutes >= 60)
    {
        const long long hours = static_cast<long long>(std::chrono::duration_cast<std::chrono::hours>(duration).count());
        const long long minute_value = static_cast<long long>(minutes - (60 * hours));
        const long long second_value = static_cast<long long>(seconds - (60 * minutes));
        char buf[32];
        sprintf_s(buf, "%02lld:%02lld:%02lld", hours, minute_value, second_value);
        return std::string(buf);
    }
    
    const long long minute_value = static_cast<long long>(minutes);
    const long long second_value = static_cast<long long>(seconds - (60 * minutes));
    char buf[32];
    sprintf_s(buf, "%lld:%02lld", minute_value, second_value);
    return std::string(buf);
}

float c_musicplayer::get_smooth_progress()
{
    if (total_time == 0)
        return 0.0f;
        
    
    if (current_time != last_update_time)
    {
        last_update_time    = current_time;
        last_known_position = current_time;
        last_update_timestamp = (float)ImGui::GetTime();
    }
    
    float current_display_time = 0.0f;
    
    if (playback_status == 4) 
    {
        float time_since_update = (float)ImGui::GetTime() - last_update_timestamp;
        uint64_t interpolated = last_known_position + (uint64_t)(time_since_update * 10000000.0);
        if (interpolated > total_time) interpolated = total_time;
        current_display_time = (float)interpolated / 10000000.0f;
    }
    else
    {
        current_display_time = (float)current_time / 10000000.0f;
    }
    
    float total_secs = (float)total_time / 10000000.0f;
    if (total_secs <= 0.0f) return 0.0f;
    return ImClamp(current_display_time / total_secs, 0.0f, 1.0f);
}

void c_musicplayer::update_thumbnail_texture()
{
    if (!thumbnail_updated || !thumbnail_buffer || thumbnail_size == 0)
        return;
        
    ID3D11ShaderResourceView* srv = nullptr;
    if (LoadTextureFromMemory((const unsigned char*)thumbnail_buffer, thumbnail_size, &srv, &thumbnail_width, &thumbnail_height))
    {
        if (thumbnail_texture)
        {
            thumbnail_texture->Release();
        }
        thumbnail_texture = srv;
        
        
        extract_dominant_colors();
    }
    
    thumbnail_updated = false;
}

void c_musicplayer::extract_dominant_colors()
{
    if (!thumbnail_buffer || thumbnail_size == 0)
        return;
        
    try
    {
        
        int width, height, channels;
        unsigned char* pixels = stbi_load_from_memory(
            (const unsigned char*)thumbnail_buffer, 
            thumbnail_size, 
            &width, 
            &height, 
            &channels, 
            3  
        );
        
        if (!pixels)
            return;
            
        
        struct ColorSample { int r, g, b, count; };
        ColorSample samples[9] = {}; 
        
        int step_x = width / 3;
        int step_y = height / 3;
        
        for (int grid_y = 0; grid_y < 3; grid_y++)
        {
            for (int grid_x = 0; grid_x < 3; grid_x++)
            {
                int idx = grid_y * 3 + grid_x;
                
                
                int start_x = grid_x * step_x;
                int start_y = grid_y * step_y;
                int end_x = start_x + step_x;
                int end_y = start_y + step_y;
                
                
                for (int y = start_y; y < end_y; y += 4)
                {
                    for (int x = start_x; x < end_x; x += 4)
                    {
                        if (x >= width || y >= height)
                            continue;
                            
                        int pixel_idx = (y * width + x) * 3;
                        samples[idx].r += pixels[pixel_idx + 0];
                        samples[idx].g += pixels[pixel_idx + 1];
                        samples[idx].b += pixels[pixel_idx + 2];
                        samples[idx].count++;
                    }
                }
                
                
                if (samples[idx].count > 0)
                {
                    samples[idx].r /= samples[idx].count;
                    samples[idx].g /= samples[idx].count;
                    samples[idx].b /= samples[idx].count;
                }
            }
        }
        
        
        int best_indices[3] = {0, 1, 2};
        float best_sats[3] = {0, 0, 0};
        
        for (int i = 0; i < 9; i++)
        {
            
            int max_c = (std::max)(samples[i].r, (std::max)(samples[i].g, samples[i].b));
            int min_c = (std::min)(samples[i].r, (std::min)(samples[i].g, samples[i].b));
            float sat = 0.0f;
            if (max_c > 0)
                sat = (float)(max_c - min_c) / (float)max_c;
            
            for (int j = 0; j < 3; j++)
            {
                if (sat > best_sats[j])
                {
                    
                    for (int k = 2; k > j; k--)
                    {
                        best_sats[k] = best_sats[k-1];
                        best_indices[k] = best_indices[k-1];
                    }
                    best_sats[j] = sat;
                    best_indices[j] = i;
                    break;
                }
            }
        }
        
        
        for (int i = 0; i < 3; i++)
        {
            int idx = best_indices[i];
            float r = samples[idx].r / 255.0f;
            float g = samples[idx].g / 255.0f;
            float b = samples[idx].b / 255.0f;
            
            
            float max_c = (std::max)(r, (std::max)(g, b));
            float min_c = (std::min)(r, (std::min)(g, b));
            float l = (max_c + min_c) * 0.5f;
            
            if (max_c > min_c)
            {
                float s = (l > 0.5f) ? (max_c - min_c) / (2.0f - max_c - min_c) : (max_c - min_c) / (max_c + min_c);
                s = (std::min)(s * 1.3f, 1.0f); 
                
                float c = (1.0f - std::abs(2.0f * l - 1.0f)) * s;
                float x = c * (1.0f - std::abs(std::fmod(0.0f, 2.0f) - 1.0f));
                float m = l - c * 0.5f;
                
                
                if (r >= g && r >= b)
                {
                    r = (r - min_c) / (max_c - min_c) * c + m;
                    g = (g - min_c) / (max_c - min_c) * c + m;
                    b = (b - min_c) / (max_c - min_c) * c + m;
                }
            }
            
            
            r *= 0.85f;
            g *= 0.85f;
            b *= 0.85f;
            
            dominant_colors[i][0] = r;
            dominant_colors[i][1] = g;
            dominant_colors[i][2] = b;
        }
        
        stbi_image_free(pixels);
    }
    catch (...)
    {
        
    }
}

void c_musicplayer::update_media_info(bool full_refresh)
{
    if (!initialized || !session_manager)
        return;
        
    try
    {
        if (full_refresh || !session)
            session = session_manager.GetCurrentSession();
        
        if (session)
        {
            has_media = true;
            
            if (full_refresh)
            {
            
            auto info = session.TryGetMediaPropertiesAsync().get();
            std::string new_title = winrt::to_string(info.Title());
            std::string new_artist = winrt::to_string(info.Artist());
            
            
            if (new_title != title || new_artist != artist)
            {
                title = new_title;
                artist = new_artist;
                
                if (title.empty())
                    title = "No Media Playing";
                if (artist.empty())
                    artist = "—";
                
                
                if (!title.empty() && title != "No Media Playing" && !artist.empty() && artist != "—")
                {
                    fetch_lyrics_async();
                }
                else
                {
                    lyrics_available = false;
                    synced_lyrics.clear();
                    plain_lyrics.clear();
                }
            }
            
            
            if (info.Thumbnail())
            {
                auto thumbnailStream = info.Thumbnail().OpenReadAsync().get();
                winrt::Windows::Storage::Streams::Buffer buffer = winrt::Windows::Storage::Streams::Buffer(thumbnailStream.Size());
                thumbnailStream.ReadAsync(buffer, buffer.Capacity(), winrt::Windows::Storage::Streams::InputStreamOptions::ReadAhead).get();
                
                thumbnail_size = buffer.Length();
                
                if (thumbnail_size != last_thumb_size)
                {
                    if (thumbnail_buffer)
                    {
                        free(thumbnail_buffer);
                    }
                    
                    thumbnail_buffer = malloc(buffer.Length());
                    memcpy(thumbnail_buffer, buffer.data(), buffer.Length());
                    thumbnail_updated = true;
                    last_thumb_size = thumbnail_size;
                }
            }
            else
            {
                thumbnail_size = 0;
            }
            }
            
            
            const auto playbackInfo = session.GetPlaybackInfo();
            playback_status = static_cast<int>(playbackInfo.PlaybackStatus());
            
            
            if (playbackInfo.IsShuffleActive())
            {
                shuffle_active = playbackInfo.IsShuffleActive().Value();
            }
            
            
            const auto timelineProperties = session.GetTimelineProperties();
            total_time = timelineProperties.EndTime().count();
            current_time = timelineProperties.Position().count();
        }
        else
        {
            has_media = false;
            title = "No Media Playing";
            artist = "—";
        }
    }
    catch (...)
    {
        has_media = false;
    }
}

void c_musicplayer::update()
{
    const uint64_t now = GetTickCount64();
    if (next_metadata_refresh_ms == 0)
        next_metadata_refresh_ms = now;
    if (next_timeline_refresh_ms == 0)
        next_timeline_refresh_ms = now;

    if (now >= next_metadata_refresh_ms)
    {
        update_media_info(true);
        next_metadata_refresh_ms = now + 1000;
        next_timeline_refresh_ms = now + 125;
    }
    else if (now >= next_timeline_refresh_ms)
    {
        update_media_info(false);
        next_timeline_refresh_ms = now + 125;
    }

    update_thumbnail_texture();
}

void c_musicplayer::toggle_play_pause()
{
    if (!has_media || !session)
        return;
        
    try
    {
        session.TryTogglePlayPauseAsync();
    }
    catch (...) {}
}

void c_musicplayer::skip_next()
{
    if (!has_media || !session)
        return;
        
    try
    {
        session.TrySkipNextAsync();
    }
    catch (...) {}
}

void c_musicplayer::skip_previous()
{
    if (!has_media || !session)
        return;
        
    try
    {
        session.TrySkipPreviousAsync();
    }
    catch (...) {}
}

void c_musicplayer::set_position(float progress)
{
    if (!has_media || !session || total_time == 0)
        return;
        
    try
    {
        session.TryChangePlaybackPositionAsync(static_cast<int64_t>(total_time * progress));
    }
    catch (...) {}
}

void c_musicplayer::toggle_shuffle()
{
    if (!has_media || !session)
        return;
        
    try
    {
        
        const auto playbackInfo = session.GetPlaybackInfo();
        bool currentShuffleState = false;
        
        if (playbackInfo.IsShuffleActive())
        {
            currentShuffleState = playbackInfo.IsShuffleActive().Value();
        }
        
        
        session.TryChangeShuffleActiveAsync(!currentShuffleState);
    }
    catch (...) {}
}

void c_musicplayer::render()
{
    if (!gui || !draw || !font)
        return;
    
    
    float anim_alpha = var->gui.spotify_spawn_alpha;
    if (anim_alpha <= 0.01f)
        return;
    
    float scale = 0.85f + (anim_alpha * 0.15f);
    
    if (!initialized)
        setup();
    
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    
    try
    {
        ImGuiIO& io = ImGui::GetIO();
        
        
        auto* backend_data = GetDX11Data();
        if (backend_data && backend_data->pd3dDevice)
        {
            ambient_bg.init(backend_data->pd3dDevice);
        }
        
        
        ImFont* text_font = font->get(main_font_data, 13.0f);
        ImFont* title_font = font->get(main_font_data, 15.0f);
        if (!text_font || !title_font)
            return;
        
        
        float base_height = SCALE(110.0f);
        float lyrics_expanded_height = SCALE(350.0f);
        float target_height = lyrics_expanded ? lyrics_expanded_height : base_height;
        gui->easing(lyrics_height, target_height, 12.0f, dynamic_easing);
        
        float width = SCALE(380.0f);
        float height = lyrics_height;
        float padding = SCALE(12.0f);
        
        ImVec2 pos  = player_pos;
        ImVec2 size = ImVec2(width, height);
        
        
        ImVec2 center     = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        ImVec2 scaled_pos = ImVec2(
            center.x + (pos.x - center.x) * scale,
            center.y + (pos.y - center.y) * scale);
        ImVec2 scaled_size = ImVec2(size.x * scale, size.y * scale);
        
        ImRect player_rect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y));
        
        
        bool hovered   = player_rect.Contains(ImGui::GetMousePos());
        bool menu_open = var->gui.menu_open;
        
        if (menu_open && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            is_dragging = true;
            drag_offset = ImVec2(ImGui::GetMousePos().x - pos.x, ImGui::GetMousePos().y - pos.y);
        }
        
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            is_dragging = false;
        
        if (is_dragging)
        {
            player_pos.x = ImClamp(ImGui::GetMousePos().x - drag_offset.x, 0.0f, io.DisplaySize.x - width);
            player_pos.y = ImClamp(ImGui::GetMousePos().y - drag_offset.y, 0.0f, io.DisplaySize.y - height);
            pos = player_pos;
            
            
            center     = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
            scaled_pos = ImVec2(center.x + (pos.x - center.x) * scale,
                                center.y + (pos.y - center.y) * scale);
            scaled_size = ImVec2(size.x * scale, size.y * scale);
            player_rect = ImRect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y));
        }
        
        gui->easing(hover_alpha, hovered ? 1.0f : 0.0f, 10.0f, dynamic_easing);
        
        
        float shadow_blur = SCALE(12.0f);
        for (int s = 0; s < 6; s++)
        {
            float progress   = (float)s / 6.0f;
            float alpha      = (1.0f - progress) * 0.25f * anim_alpha;
            float blur_amount = shadow_blur * progress;
            
            draw_list->AddRectFilled(
                ImVec2(scaled_pos.x - blur_amount, scaled_pos.y - blur_amount),
                ImVec2(scaled_pos.x + scaled_size.x + blur_amount, scaled_pos.y + scaled_size.y + blur_amount),
                IM_COL32(0, 0, 0, (int)(alpha * 255)),
                SCALE(12.0f) + blur_amount
            );
        }
        
        
        draw_list->PushClipRect(scaled_pos, ImVec2(scaled_pos.x + scaled_size.x, scaled_pos.y + scaled_size.y), true);
        
        
        
        
        float dx = scaled_pos.x - pos.x;
        float dy = scaled_pos.y - pos.y;
        pos  = scaled_pos;
        size = scaled_size;
        
        float corner_radius = SCALE(12.0f);
        
        
        if (backend_data && backend_data->pd3dDeviceContext)
        {
            ambient_bg.render(backend_data->pd3dDeviceContext, pos, size, dominant_colors);
            
            
            if (ambient_bg.shader_resource)
            {
                float u_max = size.x / c_ambient_background::TEX_WIDTH;
                float v_max = size.y / c_ambient_background::TEX_HEIGHT;
                
                draw_list->AddImageRounded(
                    (ImTextureID)ambient_bg.shader_resource,
                    pos,
                    ImVec2(pos.x + size.x, pos.y + size.y),
                    ImVec2(0, 0),
                    ImVec2(u_max, v_max),
                    IM_COL32(255, 255, 255, 255),
                    corner_radius
                );
            }
        }
        float time = (float)ImGui::GetTime();
        float anim_offset = sinf(time * 0.5f) * 0.15f + 0.85f;

        ImVec4 gray_base = ImVec4(18.0f / 255.0f, 18.0f / 255.0f, 22.0f / 255.0f, 1.0f);
        ImVec4 album_color1 = ImVec4(dominant_colors[0][0], dominant_colors[0][1], dominant_colors[0][2], 1.0f);
        ImVec4 album_color2 = ImVec4(dominant_colors[1][0], dominant_colors[1][1], dominant_colors[1][2], 1.0f);

        float gray_amount = 0.7f * anim_offset;
        float color_amount = 0.3f * anim_offset;

        ImVec4 gradient_top = ImVec4(
            gray_base.x * gray_amount + album_color1.x * color_amount,
            gray_base.y * gray_amount + album_color1.y * color_amount,
            gray_base.z * gray_amount + album_color1.z * color_amount,
            0.78f
        );

        ImVec4 gradient_bottom = ImVec4(
            gray_base.x * gray_amount + album_color2.x * color_amount,
            gray_base.y * gray_amount + album_color2.y * color_amount,
            gray_base.z * gray_amount + album_color2.z * color_amount,
            0.46f
        );

        draw_list->AddRectFilled(
            pos,
            ImVec2(pos.x + size.x, pos.y + size.y),
            draw->get_clr(gradient_top),
            corner_radius
        );
        draw_list->AddRectFilled(
            pos,
            ImVec2(pos.x + size.x, pos.y + size.y),
            draw->get_clr(gradient_bottom),
            corner_radius
        );

        ImVec4 glow_color = ImVec4(album_color1.x, album_color1.y, album_color1.z, 0.18f);
        draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), draw->get_clr(glow_color), corner_radius, 0, SCALE(1.2f));
        
        
        const float cover_size = SCALE(86.0f);
        const ImVec2 cover_pos = ImVec2(pos.x + padding, pos.y + padding);
        
        if (thumbnail_texture && thumbnail_size > 0)
        {
            
            draw_list->AddImageRounded(
                (ImTextureID)thumbnail_texture,
                cover_pos,
                ImVec2(cover_pos.x + cover_size, cover_pos.y + cover_size),
                ImVec2(0, 0),
                ImVec2(1, 1),
                IM_COL32(255, 255, 255, 255),
                SCALE(6.0f)
            );
        }
        else
        {
            
            draw_list->AddRectFilled(cover_pos, ImVec2(cover_pos.x + cover_size, cover_pos.y + cover_size), IM_COL32(40, 40, 50, 255), SCALE(6.0f));
            
            const char* placeholder = "?";
            ImVec2 placeholder_size = title_font->CalcTextSizeA(36.0f, FLT_MAX, 0.0f, placeholder);
            ImVec2 placeholder_pos = ImVec2(
                cover_pos.x + (cover_size - placeholder_size.x) * 0.5f,
                cover_pos.y + (cover_size - placeholder_size.y) * 0.5f
            );
            draw_list->AddText(title_font, 36.0f, placeholder_pos, IM_COL32(100, 100, 110, 255), placeholder);
        }
        
        
        float info_x = cover_pos.x + cover_size + padding;
        float info_width = width - cover_size - padding * 3;
        
        
        const char* title_text = title.c_str();
        ImVec2 title_pos = ImVec2(info_x, pos.y + padding);
        ImVec4 text_white = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
        draw_list->AddText(text_font, 13.0f, title_pos, draw->get_clr(text_white), title_text);
        
        
        const char* artist_text = artist.c_str();
        ImVec2 artist_pos = ImVec2(info_x, title_pos.y + SCALE(16.0f));
        ImVec4 text_gray = ImVec4(0.7f, 0.7f, 0.75f, 1.0f);
        draw_list->AddText(text_font, 11.0f, artist_pos, draw->get_clr(text_gray), artist_text);
        
        
        float progress_y = artist_pos.y + SCALE(22.0f);
        float progress_width = info_width;
        float progress_height = SCALE(3.0f);
        
        ImRect progress_rect(ImVec2(info_x, progress_y), ImVec2(info_x + progress_width, progress_y + progress_height));
        bool progress_hovered = progress_rect.Contains(ImGui::GetMousePos());
        gui->easing(progress_hover, progress_hovered ? 1.0f : 0.0f, 15.0f, dynamic_easing);
        
        
        if (progress_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && total_time > 0)
        {
            float click_x = ImGui::GetMousePos().x - progress_rect.Min.x;
            float progress = ImClamp(click_x / progress_width, 0.0f, 1.0f);
            set_position(progress);
        }
        
        
        draw_list->AddRectFilled(progress_rect.Min, progress_rect.Max, IM_COL32(255, 255, 255, 30), SCALE(2.0f));
        
        
        float fill_progress = get_smooth_progress();
        float fill_width = progress_width * fill_progress;
        draw_list->AddRectFilled(
            progress_rect.Min,
            ImVec2(progress_rect.Min.x + fill_width, progress_rect.Max.y),
            IM_COL32(255, 255, 255, 200),
            SCALE(2.0f)
        );
        
        
        float time_y = progress_y + SCALE(6.0f);
        
        
        float current_display_seconds = (total_time > 0) ? ((total_time / 10000000.0f) * fill_progress) : 0.0f;
        int current_minutes = (int)(current_display_seconds / 60.0f);
        int current_seconds = (int)(current_display_seconds) % 60;
        char current_time_str[16];
        snprintf(current_time_str, sizeof(current_time_str), "%d:%02d", current_minutes, current_seconds);
        
        std::string total_time_str = format_time(total_time);
        
        draw_list->AddText(text_font, 10.0f, ImVec2(info_x, time_y), draw->get_clr(text_gray), current_time_str);
        
        ImVec2 total_size = text_font->CalcTextSizeA(10.0f, FLT_MAX, 0.0f, total_time_str.c_str());
        draw_list->AddText(text_font, 10.0f, ImVec2(info_x + progress_width - total_size.x, time_y), draw->get_clr(text_gray), total_time_str.c_str());
        
        
        float button_y = time_y + SCALE(13.0f);
        float button_size = SCALE(24.0f);
        float button_spacing = SCALE(10.0f);
        
        
        float total_buttons_width = button_size * 4 + button_spacing * 3;
        float buttons_start_x = info_x + (progress_width - total_buttons_width) * 0.5f;
        
        
        {
            ImVec2 btn_center = ImVec2(buttons_start_x + button_size * 0.5f, button_y + button_size * 0.5f);
            ImRect btn_rect(ImVec2(btn_center.x - button_size * 0.5f, btn_center.y - button_size * 0.5f), 
                           ImVec2(btn_center.x + button_size * 0.5f, btn_center.y + button_size * 0.5f));
            bool btn_hovered = btn_rect.Contains(ImGui::GetMousePos());
            gui->easing(shuffle_hover, btn_hovered ? 1.0f : 0.0f, 15.0f, dynamic_easing);
            
            if (btn_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                toggle_shuffle();
            }
            
            
            const char* icon = "\xEF\x82\x9E";
            float icon_size = 12.0f;
            ImVec2 ic_text_size = text_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
            ImVec2 ic_pos = ImVec2(
                btn_center.x - ic_text_size.x * 0.5f,
                btn_center.y - ic_text_size.y * 0.5f
            );
            
            
            ImU32 icon_color;
            if (shuffle_active)
            {
                
                int base_alpha = 220;
                int hover_alpha = (int)(shuffle_hover * 35);
                icon_color = IM_COL32(50, 255, 100, base_alpha + hover_alpha);
            }
            else
            {
                
                int alpha = (int)(180 + (shuffle_hover * 75));
                icon_color = IM_COL32(255, 255, 255, alpha);
            }
            
            draw_list->AddText(text_font, icon_size, ic_pos, icon_color, icon);
        }
        
        
        {
            ImVec2 btn_center = ImVec2(buttons_start_x + button_size + button_spacing + button_size * 0.5f, button_y + button_size * 0.5f);
            ImRect btn_rect(ImVec2(btn_center.x - button_size * 0.5f, btn_center.y - button_size * 0.5f), 
                           ImVec2(btn_center.x + button_size * 0.5f, btn_center.y + button_size * 0.5f));
            bool btn_hovered = btn_rect.Contains(ImGui::GetMousePos());
            gui->easing(prev_hover, btn_hovered ? 1.0f : 0.0f, 15.0f, dynamic_easing);
            
            if (btn_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                skip_previous();
            }
            
            
            const char* icon = "\xEF\x82\x9B";
            float icon_size = 12.0f;
            ImVec2 ic_text_size = text_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
            ImVec2 ic_pos = ImVec2(
                btn_center.x - ic_text_size.x * 0.5f,
                btn_center.y - ic_text_size.y * 0.5f
            );
            int alpha = (int)(180 + (prev_hover * 75));
            draw_list->AddText(text_font, icon_size, ic_pos, IM_COL32(255, 255, 255, alpha), icon);
        }
        
        
        {
            float play_size = SCALE(28.0f);
            ImVec2 btn_center = ImVec2(buttons_start_x + (button_size + button_spacing) * 2 + button_size * 0.5f, button_y + button_size * 0.5f);
            ImRect btn_rect(ImVec2(btn_center.x - play_size * 0.5f, btn_center.y - play_size * 0.5f),
                           ImVec2(btn_center.x + play_size * 0.5f, btn_center.y + play_size * 0.5f));
            bool btn_hovered = btn_rect.Contains(ImGui::GetMousePos());
            gui->easing(play_hover, btn_hovered ? 1.0f : 0.0f, 15.0f, dynamic_easing);
            
            if (btn_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                toggle_play_pause();
            }
            
            
            bool is_playing = (playback_status == 4);
            const char* icon = is_playing ? "\xEF\x82\x9D" : "\xEF\x82\x9C"; 
            
            float icon_size = 14.0f;
            ImVec2 ic_text_size = text_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
            ImVec2 ic_pos = ImVec2(
                btn_center.x - ic_text_size.x * 0.5f,
                btn_center.y - ic_text_size.y * 0.5f
            );
            int alpha = (int)(200 + (play_hover * 55));
            draw_list->AddText(text_font, icon_size, ic_pos, IM_COL32(255, 255, 255, alpha), icon);
        }
        
        
        {
            ImVec2 btn_center = ImVec2(buttons_start_x + (button_size + button_spacing) * 3 + button_size * 0.5f, button_y + button_size * 0.5f);
            ImRect btn_rect(ImVec2(btn_center.x - button_size * 0.5f, btn_center.y - button_size * 0.5f),
                           ImVec2(btn_center.x + button_size * 0.5f, btn_center.y + button_size * 0.5f));
            bool btn_hovered = btn_rect.Contains(ImGui::GetMousePos());
            gui->easing(next_hover, btn_hovered ? 1.0f : 0.0f, 15.0f, dynamic_easing);
            
            if (btn_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                skip_next();
            }
            
            
            const char* icon = "\xEF\x82\x9A";
            float icon_size = 12.0f;
            ImVec2 ic_text_size = text_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
            ImVec2 ic_pos = ImVec2(
                btn_center.x - ic_text_size.x * 0.5f,
                btn_center.y - ic_text_size.y * 0.5f
            );
            int alpha = (int)(180 + (next_hover * 75));
            draw_list->AddText(text_font, icon_size, ic_pos, IM_COL32(255, 255, 255, alpha), icon);
        }
        
        
        float lyrics_btn_size = SCALE(24.0f);
        float lyrics_btn_y = pos.y + base_height - SCALE(30.0f);
        ImVec2 lyrics_btn_pos = ImVec2(pos.x + width - SCALE(34.0f), lyrics_btn_y);
        ImRect lyrics_btn_rect(lyrics_btn_pos, ImVec2(lyrics_btn_pos.x + lyrics_btn_size, lyrics_btn_pos.y + lyrics_btn_size));
        bool lyrics_btn_hovered = lyrics_btn_rect.Contains(ImGui::GetMousePos());
        
        
        gui->easing(lyrics_hover, lyrics_btn_hovered ? 1.0f : 0.0f, 15.0f, dynamic_easing);
        
        
        if (lyrics_btn_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            lyrics_expanded = !lyrics_expanded;
        }
        
        
        const char* icon_text = "aA";
        float icon_size = 11.0f;
        ImVec2 icon_text_size = text_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon_text);
        ImVec2 icon_pos = ImVec2(
            lyrics_btn_pos.x + (lyrics_btn_size - icon_text_size.x) * 0.5f,
            lyrics_btn_pos.y + (lyrics_btn_size - icon_text_size.y) * 0.5f - SCALE(2.0f)
        );
        
        
        int base_alpha = lyrics_expanded ? 255 : 150;
        int dot_alpha = (int)(base_alpha + lyrics_hover * (255 - base_alpha));
        draw_list->AddText(text_font, icon_size, icon_pos, IM_COL32(255, 255, 255, dot_alpha), icon_text);
        
        
        if (lyrics_expanded)
        {
            float dot_radius = SCALE(1.5f);
            ImVec2 dot_pos = ImVec2(
                lyrics_btn_pos.x + lyrics_btn_size * 0.5f,
                lyrics_btn_pos.y + lyrics_btn_size * 0.5f + SCALE(7.0f)
            );
            draw_list->AddCircleFilled(dot_pos, dot_radius, IM_COL32(255, 255, 255, 255));
        }
        
        
        if (lyrics_expanded && lyrics_height > base_height + SCALE(20.0f))
        {
            float lyrics_content_y = pos.y + base_height + SCALE(10.0f);
            float lyrics_content_height = lyrics_height - base_height - SCALE(20.0f);
            ImVec2 lyrics_area_pos = ImVec2(pos.x + SCALE(20.0f), lyrics_content_y);
            ImVec2 lyrics_area_size = ImVec2(width - SCALE(40.0f), lyrics_content_height);
            
            
            draw_list->PushClipRect(lyrics_area_pos, ImVec2(lyrics_area_pos.x + lyrics_area_size.x, lyrics_area_pos.y + lyrics_area_size.y), true);
            
            if (!lyrics_available)
            {
                if (lyrics_fetching)
                {
                    
                    float time = (float)ImGui::GetTime();
                    float dot_radius = SCALE(4.0f);
                    float spacing = SCALE(14.0f);
                    ImVec2 center = ImVec2(
                        lyrics_area_pos.x + lyrics_area_size.x * 0.5f,
                        lyrics_area_pos.y + lyrics_area_size.y * 0.5f
                    );
                    
                    for (int d = 0; d < 3; d++)
                    {
                        float phase = time * 3.0f - d * 0.5f;
                        float scale = 0.6f + 0.4f * (sinf(phase) * 0.5f + 0.5f);
                        float alpha = 0.4f + 0.6f * (sinf(phase) * 0.5f + 0.5f);
                        ImVec2 dot_pos = ImVec2(center.x + (d - 1) * spacing, center.y);
                        draw_list->AddCircleFilled(dot_pos, dot_radius * scale, IM_COL32(255, 255, 255, (int)(alpha * 255)));
                    }
                }
                else
                {
                    
                    const char* msg = "No lyrics available";
                    ImVec2 msg_size = text_font->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, msg);
                    ImVec2 msg_pos = ImVec2(
                        lyrics_area_pos.x + (lyrics_area_size.x - msg_size.x) * 0.5f,
                        lyrics_area_pos.y + (lyrics_area_size.y - msg_size.y) * 0.5f
                    );
                    draw_list->AddText(text_font, 13.0f, msg_pos, IM_COL32(180, 180, 180, 120), msg);
                }
            }
            else if (lyrics_synced && !synced_lyrics.empty())
            {
                
                
                float currentDisplayTime = 0.0f;
                
                if (total_time > 0)
                {
                    if (current_time != last_update_time)
                    {
                        last_update_time      = current_time;
                        last_known_position   = current_time;
                        last_update_timestamp = (float)ImGui::GetTime();
                    }
                    
                    if (playback_status == 4) 
                    {
                        float timeSinceUpdate = (float)ImGui::GetTime() - last_update_timestamp;
                        uint64_t interpolated = last_known_position + (uint64_t)(timeSinceUpdate * 10000000.0);
                        if (interpolated > total_time) interpolated = total_time;
                        currentDisplayTime = (float)interpolated / 10000000.0f;
                    }
                    else
                    {
                        currentDisplayTime = (float)current_time / 10000000.0f;
                    }
                }
                
                
                int currentLineIndex = -1;
                for (int i = 0; i < (int)synced_lyrics.size(); i++)
                {
                    
                    float line_time_sec = (float)synced_lyrics[i].time_ms / 1000.0f;
                    if (currentDisplayTime >= line_time_sec)
                        currentLineIndex = i;
                    else
                        break;
                }
                current_lyric_line = currentLineIndex;
                
                float line_height = SCALE(34.0f);
                float center_y = lyrics_area_pos.y + lyrics_area_size.y * 0.5f;
                
                
                if (current_lyric_line >= 0)
                    lyrics_scroll_target = (float)current_lyric_line * line_height;
                else
                    lyrics_scroll_target = 0.0f;
                
                
                static int s_last_lyric_line = -1;
                if (current_lyric_line != s_last_lyric_line)
                {
                    lyrics_scroll_y   = lyrics_scroll_target;
                    s_last_lyric_line = current_lyric_line;
                }
                
                
                float dt = ImGui::GetIO().DeltaTime;
                if (dt > 0.0f && dt < 0.1f)
                    lyrics_scroll_y += (lyrics_scroll_target - lyrics_scroll_y) * dt * 20.0f;
                
                for (int i = 0; i < (int)synced_lyrics.size(); i++)
                {
                    if (synced_lyrics[i].text.empty()) continue;
                    
                    float line_y = center_y + ((float)i * line_height) - lyrics_scroll_y - line_height * 0.5f;
                    
                    if (line_y + line_height < lyrics_area_pos.y || line_y > lyrics_area_pos.y + lyrics_area_size.y)
                        continue;
                    
                    float alpha;
                    float font_size;
                    int dist = (current_lyric_line >= 0) ? abs(i - current_lyric_line) : 999;
                    
                    if (current_lyric_line < 0)
                    {
                        alpha = 0.2f; font_size = SCALE(13.0f);
                    }
                    else if (i == current_lyric_line)
                    {
                        alpha = 1.0f; font_size = SCALE(15.0f);
                    }
                    else if (i < current_lyric_line)
                    {
                        if (dist == 1)      { alpha = 0.45f; font_size = SCALE(13.5f); }
                        else if (dist == 2) { alpha = 0.3f;  font_size = SCALE(13.0f); }
                        else                { alpha = 0.15f; font_size = SCALE(13.0f); }
                    }
                    else
                    {
                        if (dist == 1)      { alpha = 0.5f;  font_size = SCALE(13.5f); }
                        else if (dist == 2) { alpha = 0.35f; font_size = SCALE(13.0f); }
                        else                { alpha = 0.2f;  font_size = SCALE(13.0f); }
                    }
                    
                    const char* txt = synced_lyrics[i].text.c_str();
                    ImVec2 txt_size = text_font->CalcTextSizeA(font_size, FLT_MAX, lyrics_area_size.x, txt);
                    ImVec2 txt_pos  = ImVec2(
                        lyrics_area_pos.x + (lyrics_area_size.x - txt_size.x) * 0.5f,
                        line_y
                    );
                    
                    
                    if (i == current_lyric_line)
                    {
                        draw_list->AddText(text_font, font_size, ImVec2(txt_pos.x - 1, txt_pos.y),     IM_COL32(255,255,255,35), txt, nullptr, lyrics_area_size.x);
                        draw_list->AddText(text_font, font_size, ImVec2(txt_pos.x + 1, txt_pos.y),     IM_COL32(255,255,255,35), txt, nullptr, lyrics_area_size.x);
                        draw_list->AddText(text_font, font_size, ImVec2(txt_pos.x,     txt_pos.y - 1), IM_COL32(255,255,255,35), txt, nullptr, lyrics_area_size.x);
                        draw_list->AddText(text_font, font_size, ImVec2(txt_pos.x,     txt_pos.y + 1), IM_COL32(255,255,255,35), txt, nullptr, lyrics_area_size.x);
                    }
                    
                    draw_list->AddText(text_font, font_size, txt_pos, IM_COL32(255,255,255,(int)(alpha*255)), txt, nullptr, lyrics_area_size.x);
                }
            }
            else if (!plain_lyrics.empty())
            {
                
                float line_height = SCALE(22.0f);
                float total_height = (float)plain_lyrics.size() * line_height;
                float progress = (total_time > 0) ? ((float)current_time / (float)total_time) : 0.0f;
                float scroll_offset = progress * (total_height - lyrics_area_size.y);
                if (scroll_offset < 0.0f) scroll_offset = 0.0f;
                
                float current_y = lyrics_area_pos.y - scroll_offset;
                
                for (size_t i = 0; i < plain_lyrics.size(); i++)
                {
                    if (current_y + line_height > lyrics_area_pos.y && current_y < lyrics_area_pos.y + lyrics_area_size.y)
                    {
                        if (!plain_lyrics[i].empty())
                        {
                            const char* line_text = plain_lyrics[i].c_str();
                            ImVec2 text_size = text_font->CalcTextSizeA(13.0f, FLT_MAX, lyrics_area_size.x, line_text);
                            ImVec2 text_pos = ImVec2(
                                lyrics_area_pos.x + (lyrics_area_size.x - text_size.x) * 0.5f,
                                current_y
                            );
                            draw_list->AddText(text_font, 13.0f, text_pos, IM_COL32(220, 220, 220, 200), line_text, nullptr, lyrics_area_size.x);
                        }
                    }
                    current_y += line_height;
                }
            }
            
            draw_list->PopClipRect();
        }
    }
    catch (...)
    {
        draw_list->PopClipRect(); 
        
    }
}

// Online lyrics helpers (url_encode, title cleanup, http_get) removed with the
// external lyrics fetcher.

void c_musicplayer::fetch_lyrics_async() {
    // Online lyrics lookup removed (external linking). Lyrics are simply unavailable.
    lyrics_available = false;
    synced_lyrics.clear();
    plain_lyrics.clear();
}

// Online lyric fetch entry points (try_fetch_from_lrclib / try_fetch_plain_lyrics) removed.
// parse_lyrics_response (JSON parser for the removed lyrics APIs) removed with them.
