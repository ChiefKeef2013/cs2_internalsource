#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

class c_shader_manager
{
private:
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    
    
    ComPtr<ID3D11VertexShader> glow_vertex_shader;
    ComPtr<ID3D11PixelShader> glow_pixel_shader;
    ComPtr<ID3D11Buffer> glow_constant_buffer;
    ComPtr<ID3D11InputLayout> glow_input_layout;
    
    bool shaders_initialized = false;

public:
    struct GlowParams
    {
        float glow_color[4];      
        float glow_radius;
        float glow_intensity;
        float time;
        float padding;
    };

    void initialize(ID3D11Device* d3d_device, ID3D11DeviceContext* d3d_context)
    {
        device = d3d_device;
        context = d3d_context;
        
        if (!compile_glow_shaders())
        {
            
            shaders_initialized = false;
            return;
        }
        
        create_constant_buffer();
        shaders_initialized = true;
    }

    bool compile_glow_shaders()
    {
        
        const char* vs_code = R"(
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

        
        const char* ps_code = R"(
            struct PS_INPUT
            {
                float4 pos : SV_POSITION;
                float4 col : COLOR0;
                float2 uv  : TEXCOORD0;
            };
            sampler sampler0 : register(s0);
            Texture2D texture0 : register(t0);
            cbuffer glowBuffer : register(b0)
            {
                float4 glowColor;
                float glowRadius;
                float glowIntensity;
                float time;
                float padding;
            };
            float4 main(PS_INPUT input) : SV_Target
            {
                float4 tex = texture0.Sample(sampler0, input.uv);
                float4 baseColor = tex * input.col;
                
                // Multi-tap Glow Sampling
                float glow = 0.0f;
                const int samples = 12;
                const int angles = 8;
                
                [unroll]
                for (int i = 1; i <= samples; i++)
                {
                    float offset = glowRadius * (float(i) / float(samples));
                    
                    [unroll]
                    for (int a = 0; a < angles; a++)
                    {
                        float angle = (3.14159265f * 2.0f / float(angles)) * float(a);
                        float2 dir = float2(cos(angle), sin(angle));
                        float2 sampleUV = input.uv + dir * offset * 0.01f; // Scale down offset
                        
                        float sampledAlpha = texture0.Sample(sampler0, sampleUV).a;
                        float falloff = 1.0f - (float(i) / float(samples));
                        falloff = pow(falloff, 2.0f); // Exponential falloff
                        
                        glow += sampledAlpha * falloff;
                    }
                }
                
                glow /= float(samples * angles);
                glow *= glowIntensity;
                
                // Subtle Pulse
                float pulse = 0.85f + 0.15f * sin(time * 2.5f);
                glow *= pulse;
                
                // Apply Glow
                float3 glowContribution = glowColor.rgb * glow;
                float3 finalColor = baseColor.rgb + glowContribution;
                float finalAlpha = max(baseColor.a, glow * 0.3f);
                
                return float4(finalColor, finalAlpha);
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
            nullptr, &glow_vertex_shader);
        device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), 
            nullptr, &glow_pixel_shader);
        
        
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        
        device->CreateInputLayout(layout, 3, vs_blob->GetBufferPointer(), 
            vs_blob->GetBufferSize(), &glow_input_layout);
        
        return true;
    }

    void create_constant_buffer()
    {
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(GlowParams);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        device->CreateBuffer(&desc, nullptr, &glow_constant_buffer);
    }

    void bind_glow_shader(const GlowParams& params)
    {
        if (!shaders_initialized) return;
        
        
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context->Map(glow_constant_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, &params, sizeof(GlowParams));
            context->Unmap(glow_constant_buffer.Get(), 0);
        }
        
        
        context->VSSetShader(glow_vertex_shader.Get(), nullptr, 0);
        context->PSSetShader(glow_pixel_shader.Get(), nullptr, 0);
        context->PSSetConstantBuffers(0, 1, glow_constant_buffer.GetAddressOf());
        context->IASetInputLayout(glow_input_layout.Get());
    }

    void unbind_glow_shader()
    {
        if (!shaders_initialized) return;
        
        
        context->VSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
    }

    bool is_initialized() const { return shaders_initialized; }
};

inline std::unique_ptr<c_shader_manager> shader_manager = std::make_unique<c_shader_manager>();
