#include <svr/log_format.hpp>

#include <d3d11_4.h>
#include <dxgi1_6.h>

#undef DrawText

#include <dwrite.h>
#include <d2d1_3.h>
#include <wrl.h>

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include "stb_image_write.h"

namespace
{
    void test_dwrite(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        HRESULT hr = ERROR_SUCCESS;

        Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_factory;
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                 __uuidof(IDWriteFactory),
                                 (IUnknown**)&dwrite_factory);

        D2D1_FACTORY_OPTIONS d2d1_factory_options;
        d2d1_factory_options.debugLevel = D2D1_DEBUG_LEVEL_ERROR;

        Microsoft::WRL::ComPtr<ID2D1Factory> d2d1_factory;
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                               d2d1_factory_options,
                               d2d1_factory.ReleaseAndGetAddressOf());

        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = 1024;
        tex_desc.Height = 1024;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        hr = device->CreateTexture2D(&tex_desc, nullptr, &tex);

        tex_desc.Usage = D3D11_USAGE_STAGING;
        tex_desc.BindFlags = 0;
        tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        tex_desc.MiscFlags = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex_dl;
        hr = device->CreateTexture2D(&tex_desc, nullptr, &tex_dl);

        Microsoft::WRL::ComPtr<IDXGISurface> dxgi_surface;
        hr = tex.As(&dxgi_surface);

        Microsoft::WRL::ComPtr<ID2D1RenderTarget> d2d1_rt;

        auto d2d1_rt_props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE,
                                                          D2D1::PixelFormat(tex_desc.Format, D2D1_ALPHA_MODE_IGNORE),
                                                          96,
                                                          96,
                                                          D2D1_RENDER_TARGET_USAGE_NONE,
                                                          D2D1_FEATURE_LEVEL_10);

        hr = d2d1_factory->CreateDxgiSurfaceRenderTarget(dxgi_surface.Get(),
                                                         d2d1_rt_props,
                                                         &d2d1_rt);

        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2d1_brush;
        hr = d2d1_rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &d2d1_brush);

        Microsoft::WRL::ComPtr<IDWriteTextFormat> text_format;
        hr = dwrite_factory->CreateTextFormat(L"Segoe UI Emoji",
                                              nullptr,
                                              DWRITE_FONT_WEIGHT_NORMAL,
                                              DWRITE_FONT_STYLE_NORMAL,
                                              DWRITE_FONT_STRETCH_NORMAL,
                                              144,
                                              L"en-us",
                                              &text_format);

        d2d1_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < 1; i++)
        {
            d2d1_rt->BeginDraw();

            d2d1_rt->Clear(D2D1::ColorF(D2D1::ColorF::Black));

            d2d1_rt->DrawText(L"\xED54",
                              1,
                              text_format.Get(),
                              D2D1::RectF(0, 0, 1024, 1024),
                              d2d1_brush.Get(),
                              D2D1_DRAW_TEXT_OPTIONS_NONE,
                              DWRITE_MEASURING_MODE_GDI_NATURAL);

            d2d1_rt->EndDraw();
        }

        auto end = std::chrono::high_resolution_clock::now();

        auto diff = end - start;
        auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff);

        svr::log("{}", diff_ms.count());

        context->CopyResource(tex_dl.Get(), tex.Get());

        D3D11_MAPPED_SUBRESOURCE mapped;
        hr = context->Map(tex_dl.Get(), 0, D3D11_MAP_READ, 0, &mapped);

        std::vector<uint8_t> pixels;
        pixels.resize(tex_desc.Width * tex_desc.Height * 4);

        auto source_ptr = static_cast<uint8_t*>(mapped.pData);
        auto dest_ptr = static_cast<uint8_t*>(pixels.data());

        for (size_t i = 0; i < tex_desc.Height; i++)
        {
            std::memcpy(dest_ptr, source_ptr, tex_desc.Width * 4);

            source_ptr += mapped.RowPitch;
            dest_ptr += tex_desc.Width * 4;
        }

        context->Unmap(tex_dl.Get(), 0);

        stbi_write_png("dwrite.png", tex_desc.Width, tex_desc.Height, 4, pixels.data(), tex_desc.Width * 4);
    }
}

int main(int argc, char* argv[])
{
    std::mutex log_mutex;

    svr::log_set_function([](void* context, const char* text)
    {
        auto log_mutex = static_cast<std::mutex*>(context);
        std::lock_guard g(*log_mutex);
        std::printf(text);
    }, &log_mutex);

    auto flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    constexpr auto minimum = D3D_FEATURE_LEVEL_11_1;

    auto levels = {
        minimum
    };

    D3D_FEATURE_LEVEL created_level;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

    D3D11CreateDevice(nullptr,
                      D3D_DRIVER_TYPE_HARDWARE,
                      nullptr,
                      flags,
                      levels.begin(),
                      levels.size(),
                      D3D11_SDK_VERSION,
                      &device,
                      &created_level,
                      &context);

    test_dwrite(device.Get(), context.Get());
}
