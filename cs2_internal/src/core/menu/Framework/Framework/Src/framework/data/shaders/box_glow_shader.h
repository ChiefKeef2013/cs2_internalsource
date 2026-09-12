#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

class c_box_glow_shader
{
private:
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    
    ComPtr<ID3D11VertexShader> vertex_shader;
    ComPtr<ID3D11PixelShader> pixel_shader;
    ComPtr<ID3D11Buffer> constant_buffer;
    ComPtr<ID3D11Buffer> vertex_buffer;
    ComPtr<ID3D11InputLayout> input_layout;
    ComPtr<ID3D11BlendState> blend_state;
    
    bool initialized = false;

    struct Vertex
    {
        float pos[2];
        float uv[2];
    };

    struct GlowConstants
    {
        float box_min[2];
        float box_max[2];
        float glow_color[4];
        float glow_intensity;
        float glow_radius;
        float inner_fade;
        float time;
    };

public:
    bool initialize(ID3D11Device* d3d_device, ID3D11DeviceContext* d3d_context)
    {
        device = d3d_device;
        context = d3d_context;
        
        if (!compile_shaders())
            return false;
        
        create_constant_buffer();
        create_blend_state();
        
        initialized = true;
        return true;
    }

    bool compile_shaders()
    {
        
        const char* vs_code = R"(
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
            
            PS_INPUT main(VS_INPUT input)
            {
                PS_INPUT output;
                output.pos = float4(input.pos, 0.0f, 1.0f);
                output.uv = input.uv;
                return output;
            }
        )";

        
        const char* ps_code = R"(
            struct PS_INPUT
            {
                float4 pos : SV_POSITION;
                float2 uv : TEXCOORD0;
            };
            
            cbuffer GlowBuffer : register(b0)
            {
                float2 boxMin;
                float2 boxMax;
                float4 glowColor;
                float glowIntensity;
                float glowRadius;
                float innerFade;
                float time;
            };
            
            // Signed Distance Function für Box
            float sdBox(float2 p, float2 b)
            {
                float2 d = abs(p) - b;
                return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
            }
            
            // Smooth Gaussian-like falloff
            float gaussian(float x, float sigma)
            {
                return exp(-(x * x) / (2.0 * sigma * sigma));
            }
            
            float4 main(PS_INPUT input) : SV_Target
            {
                // Box center und size
                float2 boxCenter = (boxMin + boxMax) * 0.5;
                float2 boxSize = (boxMax - boxMin) * 0.5;
                
                // Position relativ zur Box
                float2 p = input.uv - boxCenter;
                
                // Signed Distance zur Box
                float dist = sdBox(p, boxSize);
                
                // Outer Glow (außerhalb der Box)
                float outerGlow = 0.0;
                if (dist > 0.0)
                {
                    // Multiple Gaussian samples für smootheren Glow
                    float normalizedDist = dist / glowRadius;
                    
                    // Layered Glow für mehr Tiefe
                    outerGlow += gaussian(normalizedDist, 0.3) * 1.0;
                    outerGlow += gaussian(normalizedDist, 0.5) * 0.6;
                    outerGlow += gaussian(normalizedDist, 0.8) * 0.3;
                    
                    outerGlow *= glowIntensity;
                }
                
                // Inner Glow (innerhalb der Box, subtil)
                float innerGlow = 0.0;
                if (dist < 0.0)
                {
                    float innerDist = abs(dist) / innerFade;
                    innerGlow = (1.0 - innerDist) * 0.15 * glowIntensity;
                }
                
                // Kombiniere Glows
                float totalGlow = max(outerGlow, innerGlow);
                
                // Pulsing Animation (sehr subtil)
                float pulse = 0.95 + 0.05 * sin(time * 2.0);
                totalGlow *= pulse;
                
                // Final Color mit Alpha
                float4 finalColor = glowColor;
                finalColor.a *= totalGlow;
                
                return finalColor;
            }
        )";

        ComPtr<ID3DBlob> vs_blob, ps_blob, error_blob;
        
        
        HRESULT hr = D3DCompile(vs_code, strlen(vs_code), nullptr, nullptr, nullptr,
            "main", "vs_5_0", 0, 0, &vs_blob, &error_blob);
        
        if (FAILED(hr))
        {
            return false;
        }
        
        
        hr = D3DCompile(ps_code, strlen(ps_code), nullptr, nullptr, nullptr,
            "main", "ps_5_0", 0, 0, &ps_blob, &error_blob);
        
        if (FAILED(hr))
        {
            return false;
        }
        
        
        device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
            nullptr, &vertex_shader);
        device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
            nullptr, &pixel_shader);
        
        
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        
        device->CreateInputLayout(layout, 2, vs_blob->GetBufferPointer(),
            vs_blob->GetBufferSize(), &input_layout);
        
        return true;
    }

    void create_constant_buffer()
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(GlowConstants);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        device->CreateBuffer(&desc, nullptr, &constant_buffer);
    }

    void create_blend_state()
    {
        D3D11_BLEND_DESC blend_desc = {};
        blend_desc.RenderTarget[0].BlendEnable = TRUE;
        blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        
        device->CreateBlendState(&blend_desc, &blend_state);
    }

    void render_glow(float min_x, float min_y, float max_x, float max_y,
                     float r, float g, float b, float intensity, float radius, float time)
    {
        if (!initialized) return;
        
        
        GlowConstants constants;
        constants.box_min[0] = min_x;
        constants.box_min[1] = min_y;
        constants.box_max[0] = max_x;
        constants.box_max[1] = max_y;
        constants.glow_color[0] = r;
        constants.glow_color[1] = g;
        constants.glow_color[2] = b;
        constants.glow_color[3] = 1.0f;
        constants.glow_intensity = intensity;
        constants.glow_radius = radius;
        constants.inner_fade = 5.0f;
        constants.time = time;
        
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context->Map(constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, &constants, sizeof(GlowConstants));
            context->Unmap(constant_buffer.Get(), 0);
        }
        
        
        context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, constant_buffer.GetAddressOf());
        context->IASetInputLayout(input_layout.Get());
        
        
        float blend_factor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->OMSetBlendState(blend_state.Get(), blend_factor, 0xffffffff);
        
        
        
    }

    bool is_initialized() const { return initialized; }
};

inline std::unique_ptr<c_box_glow_shader> box_glow_shader = std::make_unique<c_box_glow_shader>();
