#include "proc_priv.h"

const s32 VID_SHADER_SIZE = 8192; // Max size one shader can be when loading.

bool ProcState::vid_init(ID3D11Device* d3d11_device)
{
    bool ret = false;
    HRESULT hr;

    vid_d3d11_device = d3d11_device;
    vid_d3d11_device->AddRef();

    vid_d3d11_device->GetImmediateContext(&vid_d3d11_context);

    if (!vid_create_d2d1())
    {
        goto rfail;
    }

    if (!vid_create_dwrite())
    {
        goto rfail;
    }

    vid_shader_mem = malloc(VID_SHADER_SIZE);

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool ProcState::vid_create_d2d1()
{
    bool ret = false;
    HRESULT hr;

    IDXGIDevice* dxgi_device = NULL;
    vid_d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&vid_d2d1_factory));

    if (FAILED(hr))
    {
        svr_log("ERROR: D2D1CreateFactory returned %#x\n", hr);
        goto rfail;
    }

    hr = vid_d2d1_factory->CreateDevice(dxgi_device, &vid_d2d1_device);

    if (FAILED(hr))
    {
        svr_log("ERROR: ID2D1Factory::CreateDevice returned %#x\n", hr);
        goto rfail;
    }

    hr = vid_d2d1_device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &vid_d2d1_context);

    if (FAILED(hr))
    {
        svr_log("ERROR: ID2D1Device::CreateDeviceContext returned %#x\n", hr);
        goto rfail;
    }

    vid_d2d1_context->CreateSolidColorBrush({}, &vid_d2d1_solid_brush);

    ret = true;
    goto rexit;

rfail:

rexit:
    svr_maybe_release(&dxgi_device);
    return ret;
}

bool ProcState::vid_create_dwrite()
{
    bool ret = false;
    HRESULT hr;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&vid_dwrite_factory);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create DirectWrite factory (#x)\n", hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void ProcState::vid_free_static()
{
    svr_maybe_release(&vid_d3d11_device);
    svr_maybe_release(&vid_d3d11_context);

    svr_maybe_release(&vid_d2d1_factory);
    svr_maybe_release(&vid_d2d1_device);
    svr_maybe_release(&vid_d2d1_context);
    svr_maybe_release(&vid_dwrite_factory);
    svr_maybe_release(&vid_d2d1_solid_brush);

    if (vid_shader_mem)
    {
        free(vid_shader_mem);
        vid_shader_mem = NULL;
    }
}

void ProcState::vid_free_dynamic()
{
}

bool ProcState::vid_load_shader(const char* name)
{
    bool ret = false;

    HANDLE h = CreateFileA(svr_va("%s\\data\\shaders\\%s", svr_resource_path, name), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        svr_log("Could not load shader %s (%lu)\n", name, GetLastError());
        goto rfail;
    }

    DWORD shader_size;
    ReadFile(h, vid_shader_mem, VID_SHADER_SIZE, &shader_size, NULL);

    vid_shader_size = shader_size;

    ret = true;
    goto rexit;

rfail:

rexit:
    if (h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h);
    }

    return ret;
}

bool ProcState::vid_create_shader(const char* name, void** shader, D3D11_SHADER_TYPE type)
{
    bool ret = false;
    HRESULT hr;

    if (!vid_load_shader(name))
    {
        goto rfail;
    }

    switch (type)
    {
        case D3D11_COMPUTE_SHADER:
        {
            hr = vid_d3d11_device->CreateComputeShader(vid_shader_mem, vid_shader_size, NULL, (ID3D11ComputeShader**)shader);
            break;
        }

        case D3D11_PIXEL_SHADER:
        {
            hr = vid_d3d11_device->CreatePixelShader(vid_shader_mem, vid_shader_size, NULL, (ID3D11PixelShader**)shader);
            break;
        }

        case D3D11_VERTEX_SHADER:
        {
            hr = vid_d3d11_device->CreateVertexShader(vid_shader_mem, vid_shader_size, NULL, (ID3D11VertexShader**)shader);
            break;
        }
    }

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create shader %s (%#x)\n", name, hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void ProcState::vid_update_constant_buffer(ID3D11Buffer* buffer, void* data, UINT size)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = vid_d3d11_context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(hr));

    memcpy(mapped.pData, data, size);

    vid_d3d11_context->Unmap(buffer, 0);
}

void ProcState::vid_clear_rtv(ID3D11RenderTargetView* rtv, float r, float g, float b, float a)
{
    float clear_color[] = { r, g, b, a };
    vid_d3d11_context->ClearRenderTargetView(rtv, clear_color);
}

s32 ProcState::vid_get_num_cs_threads(s32 unit)
{
    // Thread group divisor constant must match the thread count in the compute shaders!
    return svr_align32(unit, 8) >> 3;
}

bool ProcState::vid_start()
{
    return true;
}

void ProcState::vid_end()
{
}

D2D1_COLOR_F ProcState::vid_fill_d2d1_color(SvrVec4I color)
{
    D2D1_COLOR_F ret;
    ret.r = color.x / 255.0f;
    ret.g = color.y / 255.0f;
    ret.b = color.z / 255.0f;
    ret.a = color.w / 255.0f;
    return ret;
}

D2D1_POINT_2F ProcState::vid_fill_d2d1_pt(SvrVec2I p)
{
    return D2D1::Point2F(p.x, p.y);
}
