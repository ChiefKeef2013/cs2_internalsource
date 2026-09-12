#pragma once
#include "includes.h"
#include "../headers/config.h"
#include <chrono>
#include <d3d11.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <xdraw/xui/xui.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS

enum class keybind_mode
{
    toggle = 0,
    hold = 1,
    always = 2,
};

struct keybind_item
{
    std::string name;
    int key = 0;
    bool active = false;
    keybind_mode mode = keybind_mode::toggle;
    bool state = false;
    bool listening = false;
    float appear_alpha = 0.0f;
};

class c_keybinds
{
public:
    void render();
    void update();
    void clear_keybinds();
    void begin_sync();
    void end_sync();
    void add_keybind(const std::string& name, int key = 0,
        keybind_mode mode = keybind_mode::toggle, bool active = true, bool state = false);
    bool is_active(const std::string& name);

    ImVec2& layout_position() { return keybind_pos; }

private:
    ImVec2 keybind_pos = ImVec2(15.0f, 200.0f);
    bool is_dragging = false;
    ImVec2 drag_offset = ImVec2(0.0f, 0.0f);
    int context_item = -1;
    float context_alpha = 0.0f;
    std::vector<keybind_item> keybinds;

    std::string vk_to_string(int vk);
};

inline std::unique_ptr<c_keybinds> keybinds = std::make_unique<c_keybinds>();

struct spectator_item
{
    std::string name;
    bool active = true;
    float appear_alpha = 0.0f;
};

class c_spectatorlist
{
public:
    void render();
    void add_spectator(const std::string& name);
    void remove_spectator(const std::string& name);
    void clear_spectators();
    void begin_sync();
    void end_sync();

    ImVec2& layout_position() { return spectator_pos; }

private:
    std::vector<spectator_item> spectators;
    ImVec2 spectator_pos = ImVec2(20.0f, 150.0f);
    bool is_dragging = false;
    ImVec2 drag_offset = ImVec2(0.0f, 0.0f);
};

inline std::unique_ptr<c_spectatorlist> spectatorlist = std::make_unique<c_spectatorlist>();

class c_ambient_background
{
public:
    void init(ID3D11Device* device);
    void render(ID3D11DeviceContext* context, ImVec2 pos, ImVec2 size, float dominant_colors[3][3]);
    void cleanup();

    ID3D11ShaderResourceView* shader_resource = nullptr;

    static constexpr UINT TEX_WIDTH = 512;
    static constexpr UINT TEX_HEIGHT = 512;

private:
    ID3D11Texture2D* render_texture = nullptr;
    ID3D11RenderTargetView* render_target = nullptr;
    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11Buffer* constant_buffer = nullptr;
    ID3D11RasterizerState* rasterizer_state = nullptr;
    ID3D11BlendState* blend_state = nullptr;
    bool initialized = false;
};

class c_musicplayer
{
public:
    void setup();
    void update();
    void render();
    void shutdown();
    void toggle_play_pause();
    void skip_next();
    void skip_previous();
    void set_position(float progress);
    void toggle_shuffle();

    ImVec2& layout_position() { return player_pos; }

private:
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager session_manager{ nullptr };
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session{ nullptr };
    bool has_media = false;
    std::string title = "No Media Playing";
    std::string artist = "—";
    int playback_status = 0;
    bool shuffle_active = false;
    uint64_t total_time = 0;
    uint64_t current_time = 0;

    struct lyric_line
    {
        uint64_t time_ms;
        std::string text;
    };

    std::vector<lyric_line> synced_lyrics;
    std::vector<std::string> plain_lyrics;
    bool lyrics_available = false;
    bool lyrics_synced = false;
    bool lyrics_fetching = false;
    std::string last_lyrics_track;
    int current_lyric_line = -1;
    float lyrics_scroll_y = 0.0f;
    float lyrics_scroll_target = 0.0f;
    uint64_t last_update_time = 0;
    uint64_t last_known_position = 0;
    float last_update_timestamp = 0.0f;
    uint64_t next_metadata_refresh_ms = 0;
    uint64_t next_timeline_refresh_ms = 0;
    void* thumbnail_buffer = nullptr;
    uint32_t thumbnail_size = 0;
    bool thumbnail_updated = false;
    uint32_t last_thumb_size = 0;
    ID3D11ShaderResourceView* thumbnail_texture = nullptr;
    int thumbnail_width = 0;
    int thumbnail_height = 0;
    float dominant_colors[3][3]{
        {0.47f, 0.15f, 0.70f},
        {0.15f, 0.40f, 0.85f},
        {0.80f, 0.20f, 0.45f}
    };
    c_ambient_background ambient_bg;
    ImVec2 player_pos = ImVec2(100.0f, 500.0f);
    bool is_dragging = false;
    ImVec2 drag_offset = ImVec2(0.0f, 0.0f);
    float hover_alpha = 0.0f;
    float progress_hover = 0.0f;
    float shuffle_hover = 0.0f;
    float prev_hover = 0.0f;
    float play_hover = 0.0f;
    float next_hover = 0.0f;
    float lyrics_hover = 0.0f;
    float lyrics_expand_anim = 0.0f;
    bool lyrics_expanded = false;
    float lyrics_height = 0.0f;
    bool initialized = false;

    std::string format_time(uint64_t time_100ns);
    float get_smooth_progress();
    void update_media_info(bool full_refresh);
    void update_thumbnail_texture();
    void extract_dominant_colors();
    void fetch_lyrics_async();
};

inline std::unique_ptr<c_musicplayer> musicplayer = std::make_unique<c_musicplayer>();

class c_watermark
{
public:
    void initialize(ID3D11Device* device);
    void render();
    void cleanup();
    void start_timer();

    ImVec2& layout_position() { return watermark_pos; }

private:
    void create_blur_shader(ID3D11Device* device);
    void render_blurred_background(ImVec2 pos, ImVec2 size);

    ID3D11VertexShader* vertex_shader = nullptr;
    ID3D11PixelShader* pixel_shader = nullptr;
    ID3D11Buffer* constant_buffer = nullptr;
    ID3D11SamplerState* sampler_state = nullptr;
    ID3D11BlendState* blend_state = nullptr;
    std::chrono::steady_clock::time_point injection_time{};
    bool timer_started = false;
    bool initialized = false;
    ImVec2 watermark_pos = ImVec2(15.0f, 15.0f);
    bool is_dragging = false;
    ImVec2 drag_offset = ImVec2(0.0f, 0.0f);
    int dragging_item = -1;
    ImVec2 item_drag_offset = ImVec2(0.0f, 0.0f);
    float item_offsets[4]{};
    bool item_offsets_seeded[4]{};
    bool context_open = false;
    ImVec2 context_pos = ImVec2(0.0f, 0.0f);
    float context_alpha = 0.0f;
};

inline std::unique_ptr<c_watermark> watermark = std::make_unique<c_watermark>();

class c_welcome_bar
{
public:
    void render();
};

inline std::unique_ptr<c_welcome_bar> welcome_bar = std::make_unique<c_welcome_bar>();

class c_widgets
{
public:
    void checkbox(std::string_view name, bool* value);
    void checkbox_bind(std::string_view name, xui::setting& setting);
    void render_bind_popup();
    void keybind(std::string_view id, xui::setting& setting);
    bool icon_button(std::string_view id, const char* icon, float size = 18.0f);
    void color_picker(std::string_view name, float* color);
    void slider_int(std::string_view name, int* value, int min, int max);
    void slider_float(std::string_view name, float* value, float min, float max);
    void dropdown(std::string_view name, int* value, const char* items[], int items_count, int max_visible = 0, const ImVec4* item_colors = nullptr);
    void multi_dropdown(std::string_view name, bool* values, const char* items[], int items_count, int max_visible = 0);
    void render_esp_feature_context();
    bool button(std::string_view name, float width = 0.0f);
    
    void render_draggable_buttons(ImVec2 panel_pos, ImVec2 panel_size, const char* labels[], int button_count);
    void render_esp_preview(ImVec2 preview_pos, ImVec2 preview_size);
    void draw_background_pattern(ImDrawList* draw_list, ImVec2 content_start, ImVec2 content_end);
    
    void section_header(std::string_view name);
    void separator_line();
    void spacing(float height = 8.0f);
    void render_tooltip(std::string_view text, bool hovered);
};

inline std::unique_ptr<c_widgets> widgets = std::make_unique<c_widgets>();

enum notify_type
{
    success = 0,
    warning = 1,
    info = 2
};

struct notify_state
{
    int notify_id;
    std::string_view text;
    notify_type type{ success };

    ImVec2 window_size{ 0, 0 };
    float notify_alpha{ 0 };
    bool active_notify{ true };
    float notify_timer{ 0 };
    float notify_pos{ 0 };
};

class c_notify
{
public:
    void setup_notify();

    void add_notify(std::string_view text, notify_type type);

private:
    ImVec2 render_notify(int cur_notify_value, float notify_alpha, float notify_percentage, float notify_pos, std::string_view text, notify_type type);

    float notify_time{ 300.0f };  
    int notify_count{ 0 };

    float notify_spacing{ 12.0f };  
    ImVec2 notify_padding{ 20.0f, 20.0f };  

    std::vector<notify_state> notifications;

};

inline std::unique_ptr<c_notify> notify = std::make_unique<c_notify>();
