#pragma once

namespace glow_shader
{
    
    const char* vertex_shader_hlsl = R"(
        cbuffer vertexBuffer : register(b0)
        {
            float4x4 ProjectionMatrix;
        };

        struct VS_INPUT
        {
            float2 pos : POSITION;
            float4 col : COLOR0;
            float2 uv  : TEXCOORD0;
        };

        struct PS_INPUT
        {
            float4 pos : SV_POSITION;
            float4 col : COLOR0;
            float2 uv  : TEXCOORD0;
        };

        PS_INPUT main(VS_INPUT input)
        {
            PS_INPUT output;
            output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));
            output.col = input.col;
            output.uv  = input.uv;
            return output;
        }
    )";

    
    const char* pixel_shader_hlsl = R"(
        struct PS_INPUT
        {
            float4 pos : SV_POSITION;
            float4 col : COLOR0;
            float2 uv  : TEXCOORD0;
        };

        sampler sampler0;
        Texture2D texture0;

        cbuffer glowBuffer : register(b0)
        {
            float4 glowColor;      // Glow Farbe (RGB + Intensity)
            float glowRadius;      // Glow Radius
            float glowIntensity;   // Glow Stärke
            float time;            // Für Pulsing-Animation
            float padding;
        };

        float4 main(PS_INPUT input) : SV_Target
        {
            float4 tex = texture0.Sample(sampler0, input.uv);
            float4 color = tex * input.col;
            
            // Basis Icon
            float alpha = color.a;
            
            // Glow Effekt - Multi-Sample für smoothen Glow
            float glow = 0.0f;
            int samples = 16;
            float step_size = glowRadius / float(samples);
            
            for (int i = 0; i < samples; i++)
            {
                float offset = step_size * float(i);
                
                // Radial Glow Sampling
                for (int angle = 0; angle < 8; angle++)
                {
                    float rad = (3.14159265f * 2.0f / 8.0f) * float(angle);
                    float2 dir = float2(cos(rad), sin(rad));
                    float2 sampleUV = input.uv + dir * offset;
                    
                    float4 sampledTex = texture0.Sample(sampler0, sampleUV);
                    float sampledAlpha = sampledTex.a * input.col.a;
                    
                    // Falloff
                    float falloff = 1.0f - (offset / glowRadius);
                    glow += sampledAlpha * falloff;
                }
            }
            
            glow /= float(samples * 8);
            glow *= glowIntensity;
            
            // Pulsing Animation (optional)
            float pulse = 0.8f + 0.2f * sin(time * 3.0f);
            glow *= pulse;
            
            // Glow Farbe anwenden
            float3 glowContribution = glowColor.rgb * glow * glowColor.a;
            
            // Final Color: Icon + Glow
            float3 finalColor = color.rgb + glowContribution;
            float finalAlpha = max(color.a, glow * 0.5f);
            
            return float4(finalColor, finalAlpha);
        }
    )";

    
    
    
    struct GlowShaderConstants
    {
        float glow_color[4];      
        float glow_radius;
        float glow_intensity;
        float time;
        float padding;
    };
}
