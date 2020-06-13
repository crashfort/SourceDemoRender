#pragma once
#include <svr/graphics.hpp>

#include <d3d11_4.h>
#include <dxgi1_6.h>

#undef DrawText

#include <d2d1_3.h>
#include <dwrite_3.h>

namespace svr
{
    struct graphics_shader
    {
        const char* name;

        ID3D11PixelShader* ps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11ComputeShader* cs = nullptr;
    };

    struct graphics_srv
    {
        ID3D11ShaderResourceView* srv = nullptr;
    };

    struct graphics_rtv
    {
        ID3D11RenderTargetView* rtv = nullptr;
    };

    struct graphics_uav
    {
        ID3D11UnorderedAccessView* uav = nullptr;
    };

    struct graphics_texture
    {
        const char* name;

        uint32_t width;
        uint32_t height;
        DXGI_FORMAT format;

        ID3D11Texture2D* texture = nullptr;
        ID3D11Texture2D* texture_download = nullptr;
        ID2D1RenderTarget* d2d1_rt = nullptr;

        graphics_srv shader_resource;
        graphics_rtv render_target;
        graphics_uav unordered_access;
    };

    struct graphics_buffer
    {
        const char* name;

        ID3D11Buffer* buffer = nullptr;
        ID3D11Buffer* buffer_download = nullptr;
        graphics_srv shader_resource;
        graphics_uav unordered_access;

        size_t size;
    };

    struct graphics_swapchain
    {
        const char* name;

        ID3D11Device* device = nullptr;
        IDXGISwapChain2* swapchain = nullptr;
        ID3D11Texture2D* texture = nullptr;
        graphics_rtv render_target;
    };

    struct graphics_blend_state
    {
        const char* name;
        ID3D11BlendState* blend_state = nullptr;
    };

    struct graphics_sampler_state
    {
        const char* name;
        ID3D11SamplerState* sampler_state = nullptr;
    };

    struct graphics_conversion_context
    {
        const char* name;

        graphics_conversion_context_desc desc;

        size_t used_textures;
        graphics_texture* textures[3];

        graphics_shader* cs_shader = nullptr;
        graphics_buffer* yuv_color_space_mat = nullptr;
    };

    struct graphics_text_format
    {
        const char* name;

        ID2D1RenderTarget* render_target = nullptr;
        IDWriteTextFormat* text_format = nullptr;
        ID2D1SolidColorBrush* text_brush = nullptr;
    };

    struct graphics_command_list
    {
        const char* name;
        ID3D11CommandList* list = nullptr;
    };

    __declspec(align(16)) struct graphics_motion_sample_struct
    {
        float weight;
    };
}
