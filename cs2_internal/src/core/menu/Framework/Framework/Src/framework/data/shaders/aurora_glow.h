#pragma once
#include <vector>

namespace aurora_glow
{
    struct blob_t
    {
        float x, y;
        float radius;
        float r, g, b, a;
    };

    struct aurora_params_t
    {
        float box_min_x, box_min_y;
        float box_max_x, box_max_y;
        float corner_radius;
        float glow_intensity;
        float time;
        std::vector<blob_t> blobs;
    };

    
    
    inline void render_searchbar_aurora(
        ImDrawList* draw_list,
        const ImVec2& pos,
        const ImVec2& size,
        float intensity,
        float time,
        ImU32 (*get_color_func)(const ImVec4&, float))
    {
        if (intensity < 0.01f) return;

        
        float min_x = pos.x;
        float min_y = pos.y;
        float max_x = pos.x + size.x;
        float max_y = pos.y + size.y;
        float center_x = (min_x + max_x) * 0.5f;
        float center_y = (min_y + max_y) * 0.5f;

        
        const int num_blobs = 6;
        blob_t blobs[num_blobs];

        
        float pulse1 = 0.8f + 0.2f * sinf(time * 1.5f);
        blobs[0] = {
            min_x - 2.0f + sinf(time * 0.8f) * 3.0f,
            min_y - 2.0f + cosf(time * 1.2f) * 2.0f,
            12.0f * pulse1,
            179.0f / 255.0f, 143.0f / 255.0f, 228.0f / 255.0f, 0.6f
        };

        
        float pulse2 = 0.7f + 0.3f * sinf(time * 1.8f + 1.0f);
        blobs[1] = {
            max_x + 2.0f + sinf(time * 0.9f + 2.0f) * 3.0f,
            min_y - 1.0f + cosf(time * 1.1f + 1.5f) * 2.0f,
            11.0f * pulse2,
            200.0f / 255.0f, 160.0f / 255.0f, 240.0f / 255.0f, 0.5f
        };

        
        float pulse3 = 0.85f + 0.15f * sinf(time * 2.0f + 2.0f);
        blobs[2] = {
            min_x - 1.0f + sinf(time * 1.1f + 3.0f) * 2.5f,
            max_y + 2.0f + cosf(time * 0.85f + 2.5f) * 2.0f,
            13.0f * pulse3,
            160.0f / 255.0f, 130.0f / 255.0f, 220.0f / 255.0f, 0.55f
        };

        
        float pulse4 = 0.75f + 0.25f * sinf(time * 1.6f + 3.0f);
        blobs[3] = {
            max_x + 1.5f + sinf(time * 1.05f + 4.0f) * 2.8f,
            max_y + 1.5f + cosf(time * 0.95f + 3.5f) * 2.2f,
            12.5f * pulse4,
            190.0f / 255.0f, 150.0f / 255.0f, 235.0f / 255.0f, 0.5f
        };

        
        float pulse5 = 0.9f + 0.1f * sinf(time * 2.2f + 4.0f);
        blobs[4] = {
            center_x + sinf(time * 0.7f) * 20.0f,
            min_y - 3.0f + cosf(time * 1.3f + 4.0f) * 2.0f,
            10.0f * pulse5,
            185.0f / 255.0f, 145.0f / 255.0f, 230.0f / 255.0f, 0.45f
        };

        
        float pulse6 = 0.8f + 0.2f * sinf(time * 1.9f + 5.0f);
        blobs[5] = {
            center_x + sinf(time * 0.85f + 1.5f) * 18.0f,
            max_y + 3.0f + cosf(time * 1.15f + 5.0f) * 2.0f,
            11.5f * pulse6,
            175.0f / 255.0f, 135.0f / 255.0f, 225.0f / 255.0f, 0.5f
        };

        
        for (int i = 0; i < num_blobs; i++)
        {
            const blob_t& blob = blobs[i];
            
            
            const int circle_segments = 32;
            const int radial_layers = 8;

            for (int layer = radial_layers; layer > 0; layer--)
            {
                float t = float(layer) / float(radial_layers);  
                float current_radius = blob.radius * t;
                
                
                float falloff = expf(-powf(1.0f - t, 2.0f) * 3.0f);
                float layer_alpha = blob.a * falloff * intensity;

                ImVec4 layer_color = ImVec4(blob.r, blob.g, blob.b, layer_alpha);
                ImU32 color = get_color_func(layer_color, 1.0f);

                
                draw_list->AddCircleFilled(
                    ImVec2(blob.x, blob.y),
                    current_radius,
                    color,
                    circle_segments
                );
            }
        }
    }
}
