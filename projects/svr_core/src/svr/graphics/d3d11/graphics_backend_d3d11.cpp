#include "graphics_backend_d3d11.hpp"

#include <svr/log_format.hpp>
#include <svr/media.hpp>
#include <svr/str.hpp>
#include <svr/defer.hpp>
#include <svr/swap.hpp>
#include <svr/table.hpp>
#include <svr/mem.hpp>
#include <svr/os.hpp>

#include <assert.h>

#define STBI_NO_HDR
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

static void safe_release(IUnknown* ptr)
{
    if (ptr)
    {
        ptr->Release();
    }
}

static DXGI_FORMAT convert_format(svr::graphics_format value)
{
    using namespace svr;

    switch (value)
    {
        case GRAPHICS_FORMAT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case GRAPHICS_FORMAT_R8G8B8A8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;
        case GRAPHICS_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case GRAPHICS_FORMAT_R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case GRAPHICS_FORMAT_R8_UNORM: return DXGI_FORMAT_R8_UNORM;
        case GRAPHICS_FORMAT_R8_UINT: return DXGI_FORMAT_R8_UINT;
        case GRAPHICS_FORMAT_R8G8_UINT: return DXGI_FORMAT_R8G8_UINT;
        case GRAPHICS_FORMAT_R32_UINT: return DXGI_FORMAT_R32_UINT;
    }

    assert(false);
    return DXGI_FORMAT_UNKNOWN;
}

static D3D11_USAGE convert_usage(svr::graphics_resource_usage value)
{
    using namespace svr;

    switch (value)
    {
        case GRAPHICS_USAGE_DEFAULT: return D3D11_USAGE_DEFAULT;
        case GRAPHICS_USAGE_IMMUTABLE: return D3D11_USAGE_IMMUTABLE;
        case GRAPHICS_USAGE_DYNAMIC: return D3D11_USAGE_DYNAMIC;
        case GRAPHICS_USAGE_STAGING: return D3D11_USAGE_STAGING;
    }

    return D3D11_USAGE_DEFAULT;
}

static UINT convert_buffer_type(svr::graphics_buffer_type value)
{
    using namespace svr;

    switch (value)
    {
        case GRAPHICS_BUFFER_VERTEX: return D3D11_BIND_VERTEX_BUFFER;
        case GRAPHICS_BUFFER_INDEX: return D3D11_BIND_INDEX_BUFFER;
        case GRAPHICS_BUFFER_CONSTANT: return D3D11_BIND_CONSTANT_BUFFER;
    }

    return 0;
}

static UINT convert_view_access(svr::graphics_view_access_t value)
{
    using namespace svr;

    UINT ret = 0;

    if (value & GRAPHICS_VIEW_SRV) ret |= D3D11_BIND_SHADER_RESOURCE;
    if (value & GRAPHICS_VIEW_UAV) ret |= D3D11_BIND_UNORDERED_ACCESS;
    if (value & GRAPHICS_VIEW_RTV) ret |= D3D11_BIND_RENDER_TARGET;

    return ret;
}

static UINT convert_cpu_access(svr::graphics_cpu_access_t value)
{
    using namespace svr;

    UINT ret = 0;

    if (value & GRAPHICS_CPU_ACCESS_READ) ret |= D3D11_CPU_ACCESS_READ;
    if (value & GRAPHICS_CPU_ACCESS_WRITE) ret |= D3D11_CPU_ACCESS_WRITE;

    return ret;
}

static UINT convert_misc_buffer_flags(svr::graphics_buffer_type type)
{
    UINT ret = 0;
    return ret;
}

static UINT calc_bytes_pitch(DXGI_FORMAT format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
        case DXGI_FORMAT_B8G8R8A8_UNORM: return 4;
        case DXGI_FORMAT_R8G8B8A8_UINT: return 4;
        case DXGI_FORMAT_R8G8B8A8_UNORM: return 4;
        case DXGI_FORMAT_R8_UNORM: return 1;
        case DXGI_FORMAT_R8_UINT: return 1;
        case DXGI_FORMAT_R8G8_UINT: return 2;
    }

    assert(false);
    return 0;
}

static CD3D11_VIEWPORT make_viewport(const svr::graphics_rect& rect)
{
    return CD3D11_VIEWPORT(rect.x, rect.y, rect.w, rect.h);
}

// Calculates the thread group count to use for specific dimensions.
static uint32_t calc_cs_thread_groups(uint32_t input)
{
    // Thread group divisor constant must match the thread count in the compute shaders!

    // This number is arbitrary, and may end up with better or worse performance depending
    // on the hardware.
    return ((float)input / 8.0f) + 0.5f;
}

// Converts UTF-8 to UTF-16 to a buffer in place.
// The output buffer will be null terminated.
static size_t to_utf16(const char* value, size_t value_length, wchar_t* buf, size_t buf_chars)
{
    auto length = MultiByteToWideChar(CP_UTF8, 0, value, value_length, buf, buf_chars);

    if (length < buf_chars)
    {
        buf[length] = 0;
    }

    return length;
}

struct graphics_backend_d3d11
    : svr::graphics_backend
{
    ~graphics_backend_d3d11()
    {
        safe_release(device);
        safe_release(context);
        safe_release(d2d1_factory);
        safe_release(dwrite_factory);

        if (texture_ps) destroy_shader(texture_ps);
        if (overlay_vs) destroy_shader(overlay_vs);
        if (bgr0_conversion_cs) destroy_shader(bgr0_conversion_cs);
        if (yuv420_conversion_cs) destroy_shader(yuv420_conversion_cs);
        if (nv12_conversion_cs) destroy_shader(nv12_conversion_cs);
        if (nv21_conversion_cs) destroy_shader(nv21_conversion_cs);
        if (yuv444_conversion_cs) destroy_shader(yuv444_conversion_cs);

        if (yuv_601_color_mat) destroy_buffer(yuv_601_color_mat);
        if (yuv_709_color_mat) destroy_buffer(yuv_709_color_mat);

        if (motion_sample_cs) destroy_shader(motion_sample_cs);
        if (motion_sample_cb) destroy_buffer(motion_sample_cb);

        if (opaque_bs) destroy_blend_state(opaque_bs);
        if (alpha_blend_bs) destroy_blend_state(alpha_blend_bs);
        if (additive_bs) destroy_blend_state(additive_bs);
        if (nonpremultiplied_bs) destroy_blend_state(nonpremultiplied_bs);

        if (point_ss) destroy_sampler_state(point_ss);
        if (linear_ss) destroy_sampler_state(linear_ss);
    }

    bool create_internal(const char* resource_path)
    {
        using namespace svr;

        auto load_internal_shader = [&](const char* name, graphics_shader_type type, graphics_shader** shader)
        {
            str_builder builder;
            builder.append(resource_path);
            builder.append("data/");
            builder.append(name);

            *shader = load_shader(name, type, builder.buf);
            return *shader != nullptr;
        };

        auto create_sampler = [&](const char* name, const D3D11_SAMPLER_DESC& desc, graphics_sampler_state** sampler)
        {
            *sampler = create_sampler_state(name, desc);
            return *sampler != nullptr;
        };

        auto create_blend = [&](const char* name, const D3D11_BLEND_DESC& desc, graphics_blend_state** blend)
        {
            *blend = create_blend_state(name, desc);
            return *blend != nullptr;
        };

        auto create_cbuf = [&](const char* name, bool dynamic, const void* data, size_t size, graphics_buffer** buf)
        {
            graphics_buffer_desc desc = {};

            if (dynamic)
            {
                desc.size = size;
                desc.usage = GRAPHICS_USAGE_DYNAMIC;
                desc.type = GRAPHICS_BUFFER_CONSTANT;
                desc.cpu_access = GRAPHICS_CPU_ACCESS_WRITE;
            }

            else
            {
                desc.size = size;
                desc.usage = GRAPHICS_USAGE_IMMUTABLE;
                desc.type = GRAPHICS_BUFFER_CONSTANT;
                desc.initial_desc.data = data;
                desc.initial_desc.size = size;
            }

            *buf = create_buffer(name, desc);
            return *buf != nullptr;
        };

        if (!load_internal_shader("texture_ps.svs", GRAPHICS_SHADER_PIXEL, &texture_ps)) return false;
        if (!load_internal_shader("overlay_vs.svs", GRAPHICS_SHADER_VERTEX, &overlay_vs)) return false;
        if (!load_internal_shader("bgr0_conversion_cs.svs", GRAPHICS_SHADER_COMPUTE, &bgr0_conversion_cs)) return false;
        if (!load_internal_shader("yuv420_conversion_cs.svs", GRAPHICS_SHADER_COMPUTE, &yuv420_conversion_cs)) return false;
        if (!load_internal_shader("nv12_conversion_cs.svs", GRAPHICS_SHADER_COMPUTE, &nv12_conversion_cs)) return false;
        if (!load_internal_shader("nv21_conversion_cs.svs", GRAPHICS_SHADER_COMPUTE, &nv21_conversion_cs)) return false;
        if (!load_internal_shader("yuv444_conversion_cs.svs", GRAPHICS_SHADER_COMPUTE, &yuv444_conversion_cs)) return false;
        if (!load_internal_shader("motion_sample_cs.svs", GRAPHICS_SHADER_COMPUTE, &motion_sample_cs)) return false;

        if (!create_sampler("point_ss", make_sampler_desc(D3D11_FILTER_MIN_MAG_MIP_POINT), &point_ss)) return false;
        if (!create_sampler("linear_ss", make_sampler_desc(D3D11_FILTER_MIN_MAG_MIP_LINEAR), &linear_ss)) return false;

        if (!create_blend("opaque_bs", make_blend_desc(D3D11_BLEND_ONE, D3D11_BLEND_ZERO), &opaque_bs)) return false;
        if (!create_blend("alpha_blend_bs", make_blend_desc(D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA), &alpha_blend_bs)) return false;
        if (!create_blend("additive_bs", make_blend_desc(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE), &additive_bs)) return false;
        if (!create_blend("nonpremultiplied_bs", make_blend_desc(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA), &nonpremultiplied_bs)) return false;

        auto yuv_601_mat = media_get_color_space_matrix(MEDIA_COLOR_SPACE_YUV601);
        auto yuv_709_mat = media_get_color_space_matrix(MEDIA_COLOR_SPACE_YUV709);

        if (!create_cbuf("yuv_601_color_mat_cb", false, yuv_601_mat, sizeof(*yuv_601_mat), &yuv_601_color_mat)) return false;
        if (!create_cbuf("yuv_709_color_mat_cb", false, yuv_709_mat, sizeof(*yuv_709_mat), &yuv_709_color_mat)) return false;
        if (!create_cbuf("motion_sample_cb", true, nullptr, sizeof(graphics_motion_sample_struct), &motion_sample_cb)) return false;

        return true;
    }

    svr::graphics_swapchain* create_swapchain(const char* name, const svr::graphics_swapchain_desc& desc) override
    {
        using namespace svr;

        IDXGIDevice1* dxgi_device = nullptr;
        IDXGIAdapter* dxgi_adapter = nullptr;
        IDXGIFactory2* dxgi_factory = nullptr;
        IDXGISwapChain1* initial_swap = nullptr;
        IDXGISwapChain2* final_swap = nullptr;
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;

        defer {
            safe_release(dxgi_device);
            safe_release(dxgi_adapter);
            safe_release(dxgi_factory);
            safe_release(initial_swap);
            safe_release(final_swap);
            safe_release(texture);
            safe_release(rtv);
        };

        auto hr = device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

        if (FAILED(hr))
        {
            log("d3d11: could not create d3d11 device as a dxgi device (0x{:x})\n", hr);
            return nullptr;
        }

        hr = dxgi_device->GetAdapter(&dxgi_adapter);

        if (FAILED(hr))
        {
            log("d3d11: Could not get dxgi device adapter (0x{:x})\n", hr);
            return nullptr;
        }

        hr = dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));

        if (FAILED(hr))
        {
            log("d3d11: Could not get parent of dxgi device adapter 0x{:x}\n", hr);
            return nullptr;
        }

        DXGI_SWAP_CHAIN_DESC1 swap_desc = {};
        swap_desc.Width = desc.width;
        swap_desc.Height = desc.height;
        swap_desc.Format = convert_format(desc.format);
        swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swap_desc.SampleDesc.Count = 1;
        swap_desc.BufferCount = 2;
        swap_desc.Scaling = DXGI_SCALING_NONE;
        swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs_desc = {};
        fs_desc.Windowed = true;

        hr = dxgi_factory->CreateSwapChainForHwnd(device, (HWND)desc.window, &swap_desc, &fs_desc, nullptr, &initial_swap);

        if (FAILED(hr))
        {
            log("d3d11: Could not create swapchain for hwnd (0x{:x})\n", hr);
            return nullptr;
        }

        hr = dxgi_factory->MakeWindowAssociation((HWND)desc.window, DXGI_MWA_NO_ALT_ENTER);

        if (FAILED(hr))
        {
            log("d3d11: Could not remove window alt enter association (0x{:x})\n", hr);
            return nullptr;
        }

        hr = initial_swap->GetBuffer(0, IID_PPV_ARGS(&texture));

        if (FAILED(hr))
        {
            log("d3d11: Could not get the back buffer from the swapchain (0x{:x})\n", hr);
            return nullptr;
        }

        hr = device->CreateRenderTargetView(texture, nullptr, &rtv);

        if (FAILED(hr))
        {
            log("d3d11: Could not create a rtv from swapchain back buffer (0x{:x})\n", hr);
            return nullptr;
        }

        hr = initial_swap->QueryInterface(IID_PPV_ARGS(&final_swap));

        if (FAILED(hr))
        {
            log("d3d11: Could not query initial swapchain as a newer swapchain (0x{:x})\n", hr);
            return nullptr;
        }

        auto ret = new graphics_swapchain;
        ret->name = name;

        swap_ptr(ret->swapchain, final_swap);
        swap_ptr(ret->texture, texture);
        swap_ptr(ret->render_target.rtv, rtv);

        return ret;
    }

    void destroy_swapchain(svr::graphics_swapchain* ptr) override
    {
        safe_release(ptr->swapchain);
        safe_release(ptr->texture);
        safe_release(ptr->render_target.rtv);
        delete ptr;
    }

    void present_swapchain(svr::graphics_swapchain* ptr) override
    {
        // Present immediately with no wait.
        ptr->swapchain->Present(0, 0);
    }

    void resize_swapchain(svr::graphics_swapchain* ptr, uint32_t width, uint32_t height) override
    {
        using namespace svr;

        // All references to the swapchain must be released prior.
        ptr->render_target.rtv->Release();
        ptr->texture->Release();

        auto hr = ptr->swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

        if (FAILED(hr))
        {
            log("d3d11: Could not resize swapchain '{}' (0x{:x})\n", ptr->name, hr);
            return;
        }

        // Create a new rtv with the resized buffer.

        hr = ptr->swapchain->GetBuffer(0, IID_PPV_ARGS(&ptr->texture));

        if (FAILED(hr))
        {
            log("d3d11: Could not get the back buffer from the swapchain '{}' after resize (0x{:x})\n", ptr->name, hr);
            return;
        }

        hr = device->CreateRenderTargetView(ptr->texture, nullptr, &ptr->render_target.rtv);

        if (FAILED(hr))
        {
            log("d3d11: Could not create a rtv from swapchain '{}' back buffer after resize (0x{:x})\n", ptr->name, hr);
        }
    }

    svr::graphics_rtv* get_swapchain_rtv(svr::graphics_swapchain* ptr) override
    {
        return &ptr->render_target;
    }

    svr::graphics_texture* create_texture(const char* name, const svr::graphics_texture_desc& desc) override
    {
        using namespace svr;

        ID3D11Texture2D* texture = nullptr;
        ID3D11Texture2D* texture_download = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11UnorderedAccessView* uav = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID2D1RenderTarget* d2d1_rt = nullptr;
        IDXGISurface* dxgi_surface = nullptr;

        defer {
            safe_release(texture);
            safe_release(texture_download);
            safe_release(srv);
            safe_release(uav);
            safe_release(rtv);
            safe_release(d2d1_rt);
            safe_release(dxgi_surface);
        };

        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = desc.width;
        tex_desc.Height = desc.height;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = convert_format(desc.format);
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Usage = convert_usage(desc.usage);
        tex_desc.BindFlags = convert_view_access(desc.view_access);
        tex_desc.CPUAccessFlags = convert_cpu_access(desc.cpu_access);

        D3D11_SUBRESOURCE_DATA initial_desc = {};
        auto initial_desc_ptr = &initial_desc;

        if (desc.initial_desc.data)
        {
            initial_desc.pSysMem = desc.initial_desc.data;
            initial_desc.SysMemPitch = desc.width * calc_bytes_pitch(tex_desc.Format);
        }

        else
        {
            initial_desc_ptr = nullptr;
        }

        auto hr = device->CreateTexture2D(&tex_desc, initial_desc_ptr, &texture);

        if (FAILED(hr))
        {
            log("d3d11: Could not create texture '{}' (0x{:x})\n", name, hr);
            return nullptr;
        }

        if (desc.view_access & GRAPHICS_VIEW_SRV)
        {
            hr = device->CreateShaderResourceView(texture, nullptr, &srv);

            if (FAILED(hr))
            {
                log("d3d11: Could not create srv for texture '{}' (0x{:x})\n", name, hr);
                return nullptr;
            }
        }

        if (desc.view_access & GRAPHICS_VIEW_UAV)
        {
            hr = device->CreateUnorderedAccessView(texture, nullptr, &uav);

            if (FAILED(hr))
            {
                log("d3d11: Could not create uav for texture '{}' (0x{:x})\n", name, hr);
                return nullptr;
            }
        }

        if (desc.view_access & GRAPHICS_VIEW_RTV)
        {
            hr = device->CreateRenderTargetView(texture, nullptr, &rtv);

            if (FAILED(hr))
            {
                log("d3d11: Could not create rtv for texture '{}' (0x{:x})\n", name, hr);
                return nullptr;
            }
        }

        if (desc.caps > 0)
        {
            graphics_cpu_access_t cpu_flags = 0;

            if (desc.caps & GRAPHICS_CAP_DOWNLOADABLE)
            {
                cpu_flags |= GRAPHICS_CPU_ACCESS_READ;
            }

            tex_desc.Usage = convert_usage(GRAPHICS_USAGE_STAGING);
            tex_desc.BindFlags = 0;
            tex_desc.CPUAccessFlags = convert_cpu_access(cpu_flags);
            tex_desc.MiscFlags = 0;

            hr = device->CreateTexture2D(&tex_desc, nullptr, &texture_download);

            if (FAILED(hr))
            {
                log("d3d11: Could not create downloadable texture for '{}' (0x{:x})\n", name, hr);
            }
        }

        // To be able to draw text using dwrite on a regular texture, a d2d1 texture
        // must be created as a passthrough for some reason.

        if (desc.text_target)
        {
            if (rtv == nullptr)
            {
                log("d3d11: Texture '{}' must be created as a render target for text interoperability\n", name);
                return nullptr;
            }

            hr = texture->QueryInterface(IID_PPV_ARGS(&dxgi_surface));

            if (FAILED(hr))
            {
                log("d3d11: Could not query texture '{}' as a dxgi surface (0x{:x})\n", name, hr);
                return nullptr;
            }

            auto d2d1_rt_props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_HARDWARE,
                                                              D2D1::PixelFormat(tex_desc.Format, D2D1_ALPHA_MODE_IGNORE),
                                                              96,
                                                              96,
                                                              D2D1_RENDER_TARGET_USAGE_NONE,
                                                              D2D1_FEATURE_LEVEL_10);

            hr = d2d1_factory->CreateDxgiSurfaceRenderTarget(dxgi_surface, d2d1_rt_props, &d2d1_rt);

            if (FAILED(hr))
            {
                log("d3d11: Could not create dxgi surface render target (0x{:x})\n", hr);
                return nullptr;
            }

            // Prefer to not use colored antialiasing in our case.
            // Looks better with rapidly changing background and for rapidly changing
            // text it is not worth either to spend more time making it look nice.
            d2d1_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

            // For general primitives, make them aliased.
            d2d1_rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        }

        auto ret = new graphics_texture;
        ret->name = name;
        ret->width = desc.width;
        ret->height = desc.height;
        ret->format = convert_format(desc.format);

        swap_ptr(ret->texture, texture);
        swap_ptr(ret->texture_download, texture_download);
        swap_ptr(ret->shader_resource.srv, srv);
        swap_ptr(ret->unordered_access.uav, uav);
        swap_ptr(ret->render_target.rtv, rtv);
        swap_ptr(ret->d2d1_rt, d2d1_rt);

        return ret;
    }

    svr::graphics_texture* create_texture_from_file(const char* name, const char* file, const svr::graphics_texture_load_desc& desc) override
    {
        using namespace svr;

        // Interpret everything as 4 components to fit into RGBA.
        // STB will fake the extra components if they are missing.
        const auto COMPONENTS = 4;

        int width;
        int height;
        auto data = stbi_load(file, &width, &height, nullptr, COMPONENTS);

        if (data == nullptr)
        {
            log("d3d11: Could not load image '{}' from file '{}' ({})\n", name, file, stbi_failure_reason());
            return nullptr;
        }

        defer {
            stbi_image_free(data);
        };

        // The image will always be converted by stb into RGBA.

        graphics_texture_desc texture_desc = {};
        texture_desc.width = width;
        texture_desc.height = height;
        texture_desc.format = GRAPHICS_FORMAT_R8G8B8A8_UNORM;
        texture_desc.usage = desc.usage;
        texture_desc.view_access = desc.view_access;
        texture_desc.cpu_access = desc.cpu_access;
        texture_desc.caps = desc.caps;
        texture_desc.initial_desc.data = data;
        texture_desc.initial_desc.size = width * height * calc_bytes_pitch(convert_format(texture_desc.format));

        return create_texture(name, texture_desc);
    }

    svr::graphics_texture* open_shared_texture(const char* name, svr::os_handle* handle, const svr::graphics_texture_open_desc& desc) override
    {
        using namespace svr;

        ID3D11Resource* temp_resource = nullptr;
        ID3D11Texture2D* texture = nullptr;
        ID3D11Texture2D* texture_download = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11UnorderedAccessView* uav = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;

        defer {
            safe_release(temp_resource);
            safe_release(texture);
            safe_release(texture_download);
            safe_release(srv);
            safe_release(uav);
            safe_release(rtv);
        };

        auto hr = device->OpenSharedResource((HANDLE)handle, IID_PPV_ARGS(&temp_resource));

        if (FAILED(hr))
        {
            log("d3d11: Could not open shared resource (0x{:x})\n", hr);
            return nullptr;
        }

        hr = temp_resource->QueryInterface(IID_PPV_ARGS(&texture));

        if (FAILED(hr))
        {
            log("d3d11: Could not query shared resource as a 2D texture (0x{:x})\n", hr);
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC tex_desc;
        texture->GetDesc(&tex_desc);

        if (desc.view_access & GRAPHICS_VIEW_SRV)
        {
            hr = device->CreateShaderResourceView(texture, nullptr, &srv);

            if (FAILED(hr))
            {
                log("d3d11: Could not create srv for shared texture (0x{:x})\n", hr);
                return nullptr;
            }
        }

        if (desc.view_access & GRAPHICS_VIEW_UAV)
        {
            hr = device->CreateUnorderedAccessView(texture, nullptr, &uav);

            if (FAILED(hr))
            {
                log("d3d11: Could not create uav for shared texture (0x{:x})\n", hr);
                return nullptr;
            }
        }

        if (desc.view_access & GRAPHICS_VIEW_RTV)
        {
            hr = device->CreateRenderTargetView(texture, nullptr, &rtv);

            if (FAILED(hr))
            {
                log("d3d11: Could not create rtv for shared texture (0x{:x})\n", hr);
                return nullptr;
            }
        }

        if (desc.caps > 0)
        {
            graphics_cpu_access_t cpu_flags = 0;

            if (desc.caps & GRAPHICS_CAP_DOWNLOADABLE)
            {
                cpu_flags |= GRAPHICS_CPU_ACCESS_READ;
            }

            tex_desc.Usage = convert_usage(GRAPHICS_USAGE_STAGING);
            tex_desc.BindFlags = 0;
            tex_desc.CPUAccessFlags = convert_cpu_access(cpu_flags);
            tex_desc.MiscFlags = 0;

            hr = device->CreateTexture2D(&tex_desc, nullptr, &texture_download);

            if (FAILED(hr))
            {
                log("d3d11: Could not create downloadable texture from shared resource (0x{:x})\n", hr);
                return nullptr;
            }
        }

        auto ret = new graphics_texture;
        ret->width = tex_desc.Width;
        ret->height = tex_desc.Height;
        ret->format = tex_desc.Format;

        swap_ptr(ret->texture, texture);
        swap_ptr(ret->texture_download, texture_download);
        swap_ptr(ret->shader_resource.srv, srv);
        swap_ptr(ret->unordered_access.uav, uav);
        swap_ptr(ret->render_target.rtv, rtv);

        return ret;
    }

    void destroy_texture(svr::graphics_texture* ptr) override
    {
        safe_release(ptr->texture);
        safe_release(ptr->texture_download);
        safe_release(ptr->d2d1_rt);
        safe_release(ptr->shader_resource.srv);
        safe_release(ptr->unordered_access.uav);
        safe_release(ptr->render_target.rtv);
        delete ptr;
    }

    svr::graphics_srv* get_texture_srv(svr::graphics_texture* ptr) override
    {
        assert(ptr->shader_resource.srv);
        return &ptr->shader_resource;
    }

    svr::graphics_rtv* get_texture_rtv(svr::graphics_texture* ptr) override
    {
        assert(ptr->render_target.rtv);
        return &ptr->render_target;
    }

    svr::graphics_uav* get_texture_uav(svr::graphics_texture* ptr) override
    {
        assert(ptr->unordered_access.uav);
        return &ptr->unordered_access;
    }

    void clear_rtv(svr::graphics_rtv* value, float color[4]) override
    {
        context->ClearRenderTargetView(value->rtv, color);
    }

    size_t get_texture_size(svr::graphics_texture* value) override
    {
        return value->width * value->height * calc_bytes_pitch(value->format);
    }

    svr::graphics_buffer* create_buffer(const char* name, const svr::graphics_buffer_desc& desc) override
    {
        using namespace svr;

        ID3D11Buffer* buffer = nullptr;
        ID3D11Buffer* buffer_download = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        ID3D11UnorderedAccessView* uav = nullptr;

        defer {
            safe_release(buffer);
            safe_release(buffer_download);
            safe_release(srv);
            safe_release(uav);
        };

        if (desc.type == GRAPHICS_BUFFER_CONSTANT)
        {
            if (desc.caps != 0)
            {
                log("d3d11: Constant buffer '{}' cannot have any caps\n", name);
                return nullptr;
            }
        }

        D3D11_BUFFER_DESC buffer_desc = {};
        buffer_desc.ByteWidth = desc.size;
        buffer_desc.Usage = convert_usage(desc.usage);
        buffer_desc.BindFlags = convert_view_access(desc.view_access) | convert_buffer_type(desc.type);
        buffer_desc.CPUAccessFlags = convert_cpu_access(desc.cpu_access);
        buffer_desc.MiscFlags = convert_misc_buffer_flags(desc.type);

        D3D11_SUBRESOURCE_DATA initial_desc = {};
        auto initial_desc_ptr = &initial_desc;

        if (desc.initial_desc.data > 0)
        {
            initial_desc.pSysMem = desc.initial_desc.data;
        }

        else
        {
            initial_desc_ptr = nullptr;
        }

        auto hr = device->CreateBuffer(&buffer_desc, initial_desc_ptr, &buffer);

        if (FAILED(hr))
        {
            log("d3d11: Could not create buffer '{}' (0x{:x})\n", name, hr);
            return nullptr;
        }

        if (desc.view_access & GRAPHICS_VIEW_SRV)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Format = convert_format(desc.view_desc.format);
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            srv_desc.Buffer.NumElements = desc.view_desc.elements;

            hr = device->CreateShaderResourceView(buffer, &srv_desc, &srv);

            if (FAILED(hr))
            {
                log("d3d11: could not create srv for buffer '{}' (0x{:x})\n", name, hr);
                return nullptr;
            }
        }

        if (desc.view_access & GRAPHICS_VIEW_UAV)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.Format = convert_format(desc.view_desc.format);
            uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.NumElements = desc.view_desc.elements;

            hr = device->CreateUnorderedAccessView(buffer, &uav_desc, &uav);

            if (FAILED(hr))
            {
                log("d3d11: Could not create uav for buffer '{}' (0x{:x})\n", name, hr);
                return nullptr;
            }
        }

        if (desc.caps > 0)
        {
            graphics_cpu_access_t cpu_flags = 0;

            if (desc.caps & GRAPHICS_CAP_DOWNLOADABLE)
            {
                cpu_flags |= GRAPHICS_CPU_ACCESS_READ;
            }

            buffer_desc.Usage = convert_usage(GRAPHICS_USAGE_STAGING);
            buffer_desc.BindFlags = convert_buffer_type(desc.type);
            buffer_desc.CPUAccessFlags = cpu_flags;
            buffer_desc.MiscFlags = 0;

            hr = device->CreateBuffer(&buffer_desc, nullptr, &buffer_download);

            if (FAILED(hr))
            {
                log("d3d11: Could not create downloadable buffer for '{}' (0x{:x})\n", name, hr);
                return nullptr;
            }
        }

        auto ret = new graphics_buffer;
        ret->name = name;
        ret->size = desc.size;

        swap_ptr(ret->buffer, buffer);
        swap_ptr(ret->buffer_download, buffer_download);
        swap_ptr(ret->shader_resource.srv, srv);
        swap_ptr(ret->unordered_access.uav, uav);

        return ret;
    }

    void destroy_buffer(svr::graphics_buffer* ptr) override
    {
        safe_release(ptr->buffer);
        safe_release(ptr->buffer_download);
        safe_release(ptr->shader_resource.srv);
        safe_release(ptr->unordered_access.uav);
        delete ptr;
    }

    svr::graphics_srv* get_buffer_srv(svr::graphics_buffer* ptr) override
    {
        assert(ptr->shader_resource.srv);
        return &ptr->shader_resource;
    }

    svr::graphics_uav* get_buffer_uav(svr::graphics_buffer* ptr) override
    {
        assert(ptr->unordered_access.uav);
        return &ptr->unordered_access;
    }

    size_t get_buffer_size(svr::graphics_buffer* value) override
    {
        return value->size;
    }

    void draw_overlay(svr::graphics_srv* source, svr::graphics_rtv* dest, const svr::graphics_overlay_desc& desc) override
    {
        // The overlay vertex shader automatically generates 4 vertices which covers
        // the whole render target.

        auto sampler = sampler_from_type(desc.sampler_state);
        auto blend = blend_from_type(desc.blend_state);

        ID3D11ShaderResourceView* srvs[] = {
            source->srv
        };

        ID3D11SamplerState* samplers[] = {
            sampler->sampler_state
        };

        ID3D11RenderTargetView* rtvs[] = {
            dest->rtv
        };

        auto viewport = make_viewport(desc.rect);

        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        context->VSSetShader(overlay_vs->vs, nullptr, 0);

        context->RSSetViewports(1, &viewport);

        context->PSSetShader(texture_ps->ps, nullptr, 0);
        context->PSSetShaderResources(0, 1, srvs);
        context->PSSetSamplers(0, 1, samplers);

        context->OMSetRenderTargets(1, rtvs, nullptr);
        context->OMSetBlendState(blend->blend_state, nullptr, 0xFFFFFFFF);

        context->Draw(4, 0);

        ID3D11ShaderResourceView* null_srvs[] = {
            nullptr
        };

        ID3D11SamplerState* null_samplers[] = {
            nullptr
        };

        ID3D11RenderTargetView* null_rtvs[] = {
            nullptr
        };

        context->PSSetShaderResources(0, 1, null_srvs);
        context->PSSetSamplers(0, 1, null_samplers);
        context->OMSetRenderTargets(1, null_rtvs, nullptr);
    }

    void motion_sample(const svr::graphics_motion_sample_desc& desc, svr::graphics_srv* source, svr::graphics_uav* dest, float weight) override
    {
        using namespace svr;

        ID3D11ShaderResourceView* srvs[] = {
            source->srv
        };

        ID3D11Buffer* cbufs[] = {
            motion_sample_cb->buffer
        };

        ID3D11UnorderedAccessView* uavs[] = {
            dest->uav
        };

        graphics_motion_sample_struct motion_sample_data;
        motion_sample_data.weight = weight;

        if (!update_constant_buffer(motion_sample_cb, &motion_sample_data, sizeof(motion_sample_data)))
        {
            log("d3d11: Constant buffer for motion sampling not updated\n");
            return;
        }

        context->CSSetShader(motion_sample_cs->cs, nullptr, 0);
        context->CSSetShaderResources(0, 1, srvs);
        context->CSSetConstantBuffers(0, 1, cbufs);
        context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

        context->Dispatch(calc_cs_thread_groups(desc.width), calc_cs_thread_groups(desc.height), 1);

        // @PERFORMANCE
        // If this could be avoided, performance can greatly improve (close to 2x).
        // It doesn't really work though because if removed, the motion sample effect gets discarded.
        // I don't know why the effect gets discarded, it's as if the commands are not being saved.
        // Perhaps a deferred context could be used to solve this.
        context->Flush();

        ID3D11ShaderResourceView* null_srvs[] = {
            nullptr
        };

        ID3D11Buffer* null_cbufs[] = {
            nullptr
        };

        ID3D11UnorderedAccessView* null_uavs[] = {
            nullptr
        };

        context->CSSetShader(nullptr, nullptr, 0);
        context->CSSetShaderResources(0, 1, null_srvs);
        context->CSSetConstantBuffers(0, 1, null_cbufs);
        context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
    }

    svr::graphics_conversion_context* create_conversion_context(const char* name, const svr::graphics_conversion_context_desc& desc) override
    {
        using namespace svr;

        switch (desc.dest_format)
        {
            case MEDIA_PIX_FORMAT_YUV420:
            case MEDIA_PIX_FORMAT_NV12:
            case MEDIA_PIX_FORMAT_NV21:
            case MEDIA_PIX_FORMAT_YUV444:
            {
                if ((desc.width & 1) != 0 || (desc.height & 1) != 0)
                {
                    log("d3d11: YUV conversion must have width and height dividable by 2\n");
                    return nullptr;
                }
            }
        }

        auto used_textures = media_get_plane_count(desc.dest_format);

        assert(used_textures <= 3);

        const char* texture_names[] = {
            "conversion texture 1",
            "conversion texture 2",
            "conversion texture 3",
        };

        graphics_texture* textures[3];
        memset(textures, 0, sizeof(textures));

        defer {
            if (textures[0]) destroy_texture(textures[0]);
            if (textures[1]) destroy_texture(textures[1]);
            if (textures[2]) destroy_texture(textures[2]);
        };

        auto make_tex_desc = [](graphics_format format, uint32_t width, uint32_t height)
        {
            graphics_texture_desc ret = {};
            ret.width = width;
            ret.height = height;
            ret.format = format;
            ret.usage = GRAPHICS_USAGE_DEFAULT;
            ret.view_access = GRAPHICS_VIEW_UAV;
            ret.cpu_access = GRAPHICS_CPU_ACCESS_NONE;
            ret.caps = GRAPHICS_CAP_DOWNLOADABLE;

            return ret;
        };

        switch (desc.dest_format)
        {
            case MEDIA_PIX_FORMAT_BGR0:
            {
                auto tex_desc = make_tex_desc(GRAPHICS_FORMAT_R8G8B8A8_UINT, desc.width, desc.height);
                textures[0] = create_texture(texture_names[0], tex_desc);

                if (textures[0] == nullptr)
                {
                    log("d3d11: Could not create BGR0 conversion texture\n");
                    return nullptr;
                }

                break;
            }

            case MEDIA_PIX_FORMAT_NV12:
            case MEDIA_PIX_FORMAT_NV21:
            {
                int32_t width_y;
                int32_t height_y;

                int32_t width_uv;
                int32_t height_uv;

                media_calc_plane_dimensions(desc.dest_format, 0, desc.width, desc.height, &width_y, &height_y);
                media_calc_plane_dimensions(desc.dest_format, 1, desc.width, desc.height, &width_uv, &height_uv);

                // Divide the UV plane width to account for the fact that the r8g8 format
                // has two components in two dimensions. Once in system memory the total
                // width will be correct.
                auto uv_width_div = calc_bytes_pitch(convert_format(GRAPHICS_FORMAT_R8G8_UINT));

                auto tex_desc_y = make_tex_desc(GRAPHICS_FORMAT_R8_UINT, width_y, height_y);
                auto tex_desc_uv = make_tex_desc(GRAPHICS_FORMAT_R8G8_UINT, width_uv / uv_width_div, height_uv);

                textures[0] = create_texture(texture_names[0], tex_desc_y);
                textures[1] = create_texture(texture_names[1], tex_desc_uv);

                if (textures[0] == nullptr || textures[1] == nullptr)
                {
                    log("d3d11: Could not create NV12/NV21 conversion textures\n");
                    return nullptr;
                }

                break;
            }

            case MEDIA_PIX_FORMAT_YUV420:
            case MEDIA_PIX_FORMAT_YUV444:
            {
                int32_t width_y;
                int32_t height_y;

                int32_t width_u;
                int32_t height_u;

                int32_t width_v;
                int32_t height_v;

                media_calc_plane_dimensions(desc.dest_format, 0, desc.width, desc.height, &width_y, &height_y);
                media_calc_plane_dimensions(desc.dest_format, 1, desc.width, desc.height, &width_u, &height_u);
                media_calc_plane_dimensions(desc.dest_format, 2, desc.width, desc.height, &width_v, &height_v);

                auto tex_desc_y = make_tex_desc(GRAPHICS_FORMAT_R8_UINT, width_y, height_y);
                auto tex_desc_u = make_tex_desc(GRAPHICS_FORMAT_R8_UINT, width_u, height_u);
                auto tex_desc_v = make_tex_desc(GRAPHICS_FORMAT_R8_UINT, width_v, height_v);

                textures[0] = create_texture(texture_names[0], tex_desc_y);
                textures[1] = create_texture(texture_names[1], tex_desc_u);
                textures[2] = create_texture(texture_names[2], tex_desc_v);

                if (textures[0] == nullptr || textures[1] == nullptr || textures[2] == nullptr)
                {
                    log("d3d11: Could not create YUV420/YUV444 conversion textures\n");
                    return nullptr;
                }

                break;
            }
        }

        auto ret = new graphics_conversion_context;
        ret->desc = desc;

        memset(ret->textures, 0, sizeof(ret->textures));

        for (size_t i = 0; i < used_textures; i++)
        {
            swap_ptr(ret->textures[i], textures[i]);
        }

        ret->used_textures = used_textures;

        // Set these for quick access later in runtime.

        switch (desc.dest_format)
        {
            case MEDIA_PIX_FORMAT_BGR0: ret->cs_shader = bgr0_conversion_cs; break;
            case MEDIA_PIX_FORMAT_YUV420: ret->cs_shader = yuv420_conversion_cs; break;
            case MEDIA_PIX_FORMAT_NV12: ret->cs_shader = nv12_conversion_cs; break;
            case MEDIA_PIX_FORMAT_NV21: ret->cs_shader = nv21_conversion_cs; break;
            case MEDIA_PIX_FORMAT_YUV444: ret->cs_shader = yuv444_conversion_cs; break;
        }

        switch (ret->desc.dest_color_space)
        {
            case MEDIA_COLOR_SPACE_RGB: ret->yuv_color_space_mat = nullptr; break;
            case MEDIA_COLOR_SPACE_YUV601: ret->yuv_color_space_mat = yuv_601_color_mat; break;
            case MEDIA_COLOR_SPACE_YUV709: ret->yuv_color_space_mat = yuv_709_color_mat; break;
        }

        return ret;
    }

    void destroy_conversion_context(svr::graphics_conversion_context* ptr) override
    {
        for (size_t i = 0; i < ptr->used_textures; i++)
        {
            destroy_texture(ptr->textures[i]);
        }

        delete ptr;
    }

    void convert_pixel_formats(svr::graphics_srv* source, svr::graphics_conversion_context* dest, svr::graphics_texture ** res, size_t size) override
    {
        using namespace svr;

        assert(size >= dest->used_textures);

        context->CSSetShader(dest->cs_shader->cs, nullptr, 0);

        // The input texture that should be sampled.
        ID3D11ShaderResourceView* srvs[] = {
            source->srv
        };

        context->CSSetShaderResources(0, 1, srvs);

        // Set the color space conversion matrix.
        // Only relevant for yuv video.

        if (dest->yuv_color_space_mat)
        {
            ID3D11Buffer* cbufs[] = {
                dest->yuv_color_space_mat->buffer
            };

            context->CSSetConstantBuffers(0, 1, cbufs);
        }

        switch (dest->desc.dest_format)
        {
            case MEDIA_PIX_FORMAT_BGR0:
            {
                auto tex_bgr0 = dest->textures[0];

                ID3D11UnorderedAccessView* uavs[] = {
                    tex_bgr0->unordered_access.uav,
                };

                context->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
                break;
            }

            // UV channels are interleaved in nv12 and nv21.
            case MEDIA_PIX_FORMAT_NV12:
            case MEDIA_PIX_FORMAT_NV21:
            {
                auto tex_y = dest->textures[0];
                auto tex_uv = dest->textures[1];

                ID3D11UnorderedAccessView* uavs[] = {
                    tex_y->unordered_access.uav,
                    tex_uv->unordered_access.uav,
                };

                context->CSSetUnorderedAccessViews(0, 2, uavs, nullptr);
                break;
            }

            // All separate channels.
            case MEDIA_PIX_FORMAT_YUV420:
            case MEDIA_PIX_FORMAT_YUV444:
            {
                auto tex_y = dest->textures[0];
                auto tex_u = dest->textures[1];
                auto tex_v = dest->textures[2];

                ID3D11UnorderedAccessView* uavs[] = {
                    tex_y->unordered_access.uav,
                    tex_u->unordered_access.uav,
                    tex_v->unordered_access.uav,
                };

                context->CSSetUnorderedAccessViews(0, 3, uavs, nullptr);
                break;
            }
        }

        context->Dispatch(calc_cs_thread_groups(dest->desc.width), calc_cs_thread_groups(dest->desc.height), 1);

        // Write output textures.
        for (size_t i = 0; i < dest->used_textures; i++)
        {
            res[i] = dest->textures[i];
        }

        ID3D11ShaderResourceView* null_srvs[] = {
            nullptr
        };

        ID3D11Buffer* null_cbufs[] = {
            nullptr
        };

        ID3D11UnorderedAccessView* null_uavs[] = {
            nullptr
        };

        context->CSSetShaderResources(0, 1, null_srvs);
        context->CSSetConstantBuffers(0, 1, null_cbufs);
        context->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
    }

    size_t get_conversion_texture_count(svr::graphics_conversion_context* ptr) override
    {
        return ptr->used_textures;
    }

    void get_conversion_sizes(svr::graphics_conversion_context* ptr, size_t* sizes, size_t count) override
    {
        using namespace svr;

        size_t ret = 0;

        if (count < ptr->used_textures)
        {
            log("d3d11: Could not get conversion sizes for '{}' because the input buffer is smaller than what the conversion texture count ({} < {})\n", ptr->name, count, ptr->used_textures);
            return;
        }

        for (size_t i = 0; i < ptr->used_textures; i++)
        {
            sizes[i] = get_texture_size(ptr->textures[i]);
        }
    }

    void download_texture(svr::graphics_texture* source, void* dest, size_t size) override
    {
        using namespace svr;

        assert(source->texture_download != nullptr);
        assert(get_texture_size(source) == size);

        context->CopyResource(source->texture_download, source->texture);

        D3D11_MAPPED_SUBRESOURCE mapped;
        auto hr = context->Map(source->texture_download, 0, D3D11_MAP_READ, 0, &mapped);

        if (FAILED(hr))
        {
            log("d3d11: Could not map texture '{}' to system memory (0x{:x})\n", source->name, hr);

            // Don't need to unmap if the mapping failed.
            return;
        }

        // The runtime might assign values to RowPitch and DepthPitch that are larger than anticipated
        // because there might be padding between rows and depth.

        // This pitch is used for writing to the destination.
        auto dest_pitch = source->width * calc_bytes_pitch(source->format);

        auto source_ptr = static_cast<uint8_t*>(mapped.pData);
        auto dest_ptr = static_cast<uint8_t*>(dest);

        for (uint32_t i = 0; i < source->height; i++)
        {
            memcpy(dest_ptr, source_ptr, dest_pitch);

            source_ptr += mapped.RowPitch;
            dest_ptr += dest_pitch;
        }

        context->Unmap(source->texture_download, 0);
    }

    void download_buffer(svr::graphics_buffer* source, void* dest, size_t size) override
    {
        using namespace svr;

        assert(source->buffer_download != nullptr);
        assert(source->size == size);

        context->CopyResource(source->buffer_download, source->buffer);

        D3D11_MAPPED_SUBRESOURCE mapped;
        auto hr = context->Map(source->buffer_download, 0, D3D11_MAP_READ, 0, &mapped);

        if (FAILED(hr))
        {
            log("d3d11: Could not map buffer '{}' to system memory (0x{:x})\n", source->name, hr);

            // Don't need to unmap if the mapping failed.
            return;
        }

        memcpy(dest, mapped.pData, source->size);

        context->Unmap(source->buffer_download, 0);
    }

    svr::graphics_text_format* create_text_format(const char* name, svr::graphics_texture* tex, const svr::graphics_text_format_desc& desc) override
    {
        using namespace svr;

        if (tex->d2d1_rt == nullptr)
        {
            log("d3d11: Texture is not a d2d1 render target\n");
            return nullptr;
        }

        IDWriteTextFormat* old_format = nullptr;
        IDWriteTextFormat* text_format = nullptr;
        ID2D1SolidColorBrush* text_brush = nullptr;

        defer {
            safe_release(old_format);
            safe_release(text_format);
            safe_release(text_brush);
        };

        table WEIGHTS = {
            table_pair{"thin", DWRITE_FONT_WEIGHT_THIN},
            table_pair{"extralight", DWRITE_FONT_WEIGHT_EXTRA_LIGHT},
            table_pair{"light", DWRITE_FONT_WEIGHT_LIGHT},
            table_pair{"semilight", DWRITE_FONT_WEIGHT_SEMI_LIGHT},
            table_pair{"normal", DWRITE_FONT_WEIGHT_NORMAL},
            table_pair{"medium", DWRITE_FONT_WEIGHT_MEDIUM},
            table_pair{"semibold", DWRITE_FONT_WEIGHT_SEMI_BOLD},
            table_pair{"bold", DWRITE_FONT_WEIGHT_BOLD},
            table_pair{"extrabold", DWRITE_FONT_WEIGHT_EXTRA_BOLD},
            table_pair{"black", DWRITE_FONT_WEIGHT_BLACK},
            table_pair{"extrablack", DWRITE_FONT_WEIGHT_EXTRA_BLACK},
        };

        auto weight = table_map_key_or(WEIGHTS, desc.font_weight, DWRITE_FONT_WEIGHT_NORMAL);

        table styles = {
            table_pair{"normal", DWRITE_FONT_STYLE_NORMAL},
            table_pair{"oblique", DWRITE_FONT_STYLE_OBLIQUE},
            table_pair{"italic", DWRITE_FONT_STYLE_ITALIC},
        };

        auto style = table_map_key_or(styles, desc.font_style, DWRITE_FONT_STYLE_NORMAL);

        table stretches = {
            table_pair{"undefined", DWRITE_FONT_STRETCH_UNDEFINED},
            table_pair{"ultracondensed", DWRITE_FONT_STRETCH_ULTRA_CONDENSED},
            table_pair{"extracondensed", DWRITE_FONT_STRETCH_EXTRA_CONDENSED},
            table_pair{"condensed", DWRITE_FONT_STRETCH_CONDENSED},
            table_pair{"semicondensed", DWRITE_FONT_STRETCH_SEMI_CONDENSED},
            table_pair{"normal", DWRITE_FONT_STRETCH_NORMAL},
            table_pair{"semiexpanded", DWRITE_FONT_STRETCH_SEMI_EXPANDED},
            table_pair{"expanded", DWRITE_FONT_STRETCH_EXPANDED},
            table_pair{"extraexpanded", DWRITE_FONT_STRETCH_EXTRA_EXPANDED},
            table_pair{"ultraexpanded", DWRITE_FONT_STRETCH_ULTRA_EXPANDED},
        };

        auto stretch = table_map_key_or(stretches, desc.font_stretch, DWRITE_FONT_STRETCH_NORMAL);

        table text_aligns = {
            table_pair{"leading", DWRITE_TEXT_ALIGNMENT_LEADING},
            table_pair{"trailing", DWRITE_TEXT_ALIGNMENT_TRAILING},
            table_pair{"center", DWRITE_TEXT_ALIGNMENT_CENTER},
        };

        auto text_align = table_map_key_or(text_aligns, desc.text_align, DWRITE_TEXT_ALIGNMENT_CENTER);

        table para_aligns = {
            table_pair{"near", DWRITE_PARAGRAPH_ALIGNMENT_NEAR},
            table_pair{"far", DWRITE_PARAGRAPH_ALIGNMENT_FAR},
            table_pair{"center", DWRITE_PARAGRAPH_ALIGNMENT_CENTER},
        };

        auto paragraph_align = table_map_key_or(para_aligns, desc.paragraph_align, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        auto color = D2D1::ColorF(desc.color_r / 255.0f, desc.color_g / 255.0f, desc.color_b / 255.0f, desc.color_a / 255.0f);

        auto hr = tex->d2d1_rt->CreateSolidColorBrush(color, &text_brush);

        if (FAILED(hr))
        {
            log("d3d11: Could not create text brush (0x{:x})\n", hr);
            return nullptr;
        }

        wchar_t font_family_w[128];
        to_utf16(desc.font_family, strlen(desc.font_family), font_family_w, 128);

        hr = dwrite_factory->CreateTextFormat(font_family_w, nullptr, weight, style, stretch, desc.font_size, L"en-us", &old_format);

        if (FAILED(hr))
        {
            log("d3d11: Could not create text format (0x{:x})\n", hr);
            return nullptr;
        }

        hr = old_format->QueryInterface(IID_PPV_ARGS(&text_format));

        if (FAILED(hr))
        {
            log("d3d11: Could not query text format as newer (0x{:x})\n", hr);
            return nullptr;
        }

        text_format->SetTextAlignment(text_align);
        text_format->SetParagraphAlignment(paragraph_align);

        auto ret = new graphics_text_format;
        ret->name = name;
        ret->render_target = tex->d2d1_rt;
        ret->render_target->AddRef();

        swap_ptr(ret->text_format, text_format);
        swap_ptr(ret->text_brush, text_brush);

        return ret;
    }

    void destroy_text_format(svr::graphics_text_format* ptr) override
    {
        safe_release(ptr->render_target);
        safe_release(ptr->text_format);
        safe_release(ptr->text_brush);
        delete ptr;
    }

    void draw_text(svr::graphics_text_format* ptr, const char* text, int left, int top, int right, int bottom) override
    {
        using namespace svr;

        const auto MAX_TEXT_LENGTH = 128;

        auto text_len = strlen(text);

        if (text_len > MAX_TEXT_LENGTH)
        {
            log("d3d11: Trying to draw text that is too long (length is {}, max is 127)\n", text_len);
            return;
        }

        wchar_t text_buf[MAX_TEXT_LENGTH];
        auto text_buf_len = to_utf16(text, text_len, text_buf, MAX_TEXT_LENGTH);

        auto rect = D2D1::RectF(left, top, right, bottom);

        ptr->render_target->BeginDraw();
        ptr->render_target->DrawText(text_buf, text_buf_len, ptr->text_format, rect, ptr->text_brush, D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_GDI_NATURAL);
        ptr->render_target->EndDraw();
    }

    svr::graphics_shader* create_shader(const char* name, const svr::graphics_shader_desc& desc)
    {
        using namespace svr;

        HRESULT hr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11VertexShader* vs = nullptr;
        ID3D11ComputeShader* cs = nullptr;

        if (desc.type == GRAPHICS_SHADER_PIXEL)
        {
            hr = device->CreatePixelShader(desc.data, desc.size, nullptr, &ps);
        }

        else if (desc.type == GRAPHICS_SHADER_VERTEX)
        {
            hr = device->CreateVertexShader(desc.data, desc.size, nullptr, &vs);
        }

        else if (desc.type == GRAPHICS_SHADER_COMPUTE)
        {
            hr = device->CreateComputeShader(desc.data, desc.size, nullptr, &cs);
        }

        if (FAILED(hr))
        {
            log("d3d11: Could not create shader '{}' (0x{:x})\n", name, hr);
            return nullptr;
        }

        auto ret = new graphics_shader;
        ret->ps = ps;
        ret->vs = vs;
        ret->cs = cs;

        return ret;
    }

    svr::graphics_shader* load_shader(const char* name, svr::graphics_shader_type type, const char* path)
    {
        using namespace svr;

        mem_buffer buf;

        if (!os_read_file(path, buf))
        {
            log("d3d11: Could not open shader '{}'\n", path);
            return nullptr;
        }

        defer {
            mem_destroy_buffer(buf);
        };

        graphics_shader_desc desc;
        desc.type = type;
        desc.data = buf.data;
        desc.size = buf.size;

        return create_shader(name, desc);
    }

    void destroy_shader(svr::graphics_shader* ptr)
    {
        safe_release(ptr->ps);
        safe_release(ptr->vs);
        safe_release(ptr->cs);
        delete ptr;
    }

    svr::graphics_blend_state* create_blend_state(const char* name, const D3D11_BLEND_DESC& desc)
    {
        using namespace svr;

        ID3D11BlendState* bs = nullptr;
        auto hr = device->CreateBlendState(&desc, &bs);

        if (FAILED(hr))
        {
            log("d3d11: could not create blend state '{}' (0x{:x})\n", name, hr);
            return nullptr;
        }

        auto ret = new graphics_blend_state;
        ret->name = name;
        ret->blend_state = bs;

        return ret;
    }

    void destroy_blend_state(svr::graphics_blend_state* ptr)
    {
        safe_release(ptr->blend_state);
        delete ptr;
    }

    svr::graphics_sampler_state* create_sampler_state(const char* name, const D3D11_SAMPLER_DESC& desc)
    {
        using namespace svr;

        ID3D11SamplerState* ss = nullptr;
        auto hr = device->CreateSamplerState(&desc, &ss);

        if (FAILED(hr))
        {
            log("d3d11: Could not create sampler state '{}' (0x{:x})\n", name, hr);
            return false;
        }

        auto ret = new graphics_sampler_state;
        ret->sampler_state = ss;

        return ret;
    }

    void destroy_sampler_state(svr::graphics_sampler_state* ptr)
    {
        safe_release(ptr->sampler_state);
        delete ptr;
    }

    D3D11_SAMPLER_DESC make_sampler_desc(D3D11_FILTER filter)
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = filter;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD = D3D11_FLOAT32_MAX;

        return desc;
    }

    D3D11_BLEND_DESC make_blend_desc(D3D11_BLEND source, D3D11_BLEND dest)
    {
        D3D11_BLEND_DESC ret = {};

        for (size_t i = 0; i < 8; i++)
        {
            auto& target = ret.RenderTarget[i];

            if (source != D3D11_BLEND_ONE || dest != D3D11_BLEND_ZERO)
            {
                target.BlendEnable = 1;
            }

            target.SrcBlend = source;
            target.SrcBlendAlpha = source;
            target.DestBlend = dest;
            target.DestBlendAlpha = dest;
            target.BlendOp = D3D11_BLEND_OP_ADD;
            target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        return ret;
    }

    bool update_constant_buffer(svr::graphics_buffer* ptr, const void* data, size_t size)
    {
        using namespace svr;

        // Must overwrite the entire thing in discard mode.
        assert(ptr->size == size);

        D3D11_MAPPED_SUBRESOURCE mapped;
        auto hr = context->Map(ptr->buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

        if (FAILED(hr))
        {
            log("d3d11: Could not map constant buffer '{}' (0x{:x})\n", ptr->name, hr);

            // Don't need to unmap if the mapping failed.
            return false;
        }

        memcpy(mapped.pData, data, size);

        context->Unmap(ptr->buffer, 0);

        return true;
    }

    svr::graphics_blend_state* blend_from_type(svr::graphics_blend_state_type value)
    {
        using namespace svr;

        switch (value)
        {
            case GRAPHICS_BLEND_OPAQUE: return opaque_bs;
            case GRAPHICS_BLEND_ALPHA_BLEND: return alpha_blend_bs;
            case GRAPHICS_BLEND_ADDITIVE: return additive_bs;
            case GRAPHICS_BLEND_NONPREMULTIPLIED: return nonpremultiplied_bs;
        }

        assert(false);
        return nullptr;
    }

    svr::graphics_sampler_state* sampler_from_type(svr::graphics_sampler_state_type value)
    {
        using namespace svr;

        switch (value)
        {
            case GRAPHICS_SAMPLER_POINT: return point_ss;
            case GRAPHICS_SAMPLER_LINEAR: return linear_ss;
        }

        assert(false);
        return nullptr;
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    ID2D1Factory* d2d1_factory = nullptr;
    IDWriteFactory7* dwrite_factory = nullptr;

    // Pixel shader that only reads from a texture shader resource.
    svr::graphics_shader* texture_ps = nullptr;

    // Vertex shader that fills an entire render target with no transformation in 2 dimensions.
    svr::graphics_shader* overlay_vs = nullptr;

    // Compute shaders that converts a standard texture into a media texture.
    svr::graphics_shader* bgr0_conversion_cs = nullptr;
    svr::graphics_shader* yuv420_conversion_cs = nullptr;
    svr::graphics_shader* nv12_conversion_cs = nullptr;
    svr::graphics_shader* nv21_conversion_cs = nullptr;
    svr::graphics_shader* yuv444_conversion_cs = nullptr;

    // Constant buffer that contains the matrix required for RGB to YUV conversion using the 601 color space.
    svr::graphics_buffer* yuv_601_color_mat = nullptr;

    // Constant buffer that contains the matrix required for RGB to YUV conversion using the 709 color space.
    svr::graphics_buffer* yuv_709_color_mat = nullptr;

    // Compute shader that handles multiplying together two frames by a weight.
    svr::graphics_shader* motion_sample_cs = nullptr;

    // Constant buffer that contains the weight to multiply the two frames with.
    svr::graphics_buffer* motion_sample_cb = nullptr;

    // Pre-defined blend states.
    svr::graphics_blend_state* opaque_bs = nullptr;
    svr::graphics_blend_state* alpha_blend_bs = nullptr;
    svr::graphics_blend_state* additive_bs = nullptr;
    svr::graphics_blend_state* nonpremultiplied_bs = nullptr;

    // Pre-defined sampler states.
    svr::graphics_sampler_state* point_ss = nullptr;
    svr::graphics_sampler_state* linear_ss = nullptr;
};

namespace svr
{
    graphics_backend* graphics_create_d3d11_backend(const char* resource_path)
    {
        // BGRA support needed for Direct2D interoperability.
        // It is also only intended to be used from a single thread.
        UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

        #ifndef NDEBUG
        log("d3d11: Creating debug device. Install the debug layer for verbose troubleshooting\n");
        flags |= D3D11_CREATE_DEVICE_DEBUG;
        #else
        log("d3d11: Creating release device\n");
        #endif

        // Should be good enough for all the features that we make use of.
        const auto MINIMUM_VERSION = D3D_FEATURE_LEVEL_11_0;

        D3D_FEATURE_LEVEL levels[] = {
            MINIMUM_VERSION
        };

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        ID2D1Factory* d2d1_factory = nullptr;
        IDWriteFactory7* dwrite_factory = nullptr;

        defer {
            safe_release(device);
            safe_release(context);
            safe_release(d2d1_factory);
            safe_release(dwrite_factory);
        };

        D3D_FEATURE_LEVEL created_level;

        auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 1, D3D11_SDK_VERSION, &device, &created_level, &context);

        if (FAILED(hr))
        {
            log("d3d11: Could not create d3d11 device (0x{:x})\n", hr);
            return false;
        }

        // Only support the minimum feature level and nothing else.

        if (created_level < MINIMUM_VERSION)
        {
            log("d3d11: Created device with feature level 0x{:x} but minimum is 0x{:x}\n", created_level, MINIMUM_VERSION);
            return false;
        }

        log("d3d11: Created device with feature level {}\n", created_level);

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d1_factory);

        if (FAILED(hr))
        {
            log("d3d11: Could not create d2d1 factory (0x{:x})\n", hr);
            return false;
        }

        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory7), (IUnknown**)&dwrite_factory);

        if (FAILED(hr))
        {
            log("d3d11: Could not create dwrite factory (0x{:x})\n", hr);
            return false;
        }

        auto ret = new graphics_backend_d3d11;

        swap_ptr(ret->device, device);
        swap_ptr(ret->context, context);
        swap_ptr(ret->d2d1_factory, d2d1_factory);
        swap_ptr(ret->dwrite_factory, dwrite_factory);

        // Destroy the whole thing if something internal could not be created.
        if (!ret->create_internal(resource_path))
        {
            graphics_destroy_backend(ret);
            return nullptr;
        }

        return ret;
    }
}
