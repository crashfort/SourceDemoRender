#include "encoder_priv.h"

// Conversion from game texture format to video encoder format.

const s32 VID_SHADER_SIZE = 8192; // Max size one shader can be when loading.

bool EncoderState::vid_init()
{
    bool ret = false;
    HRESULT hr;

    if (!vid_create_device())
    {
        goto rfail;
    }

    if (!vid_create_shaders())
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::vid_create_device()
{
    bool ret = false;
    HRESULT hr;

    UINT device_create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    #if SVR_DEBUG
    device_create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    // Should be good enough for all the features that we make use of.
    const D3D_FEATURE_LEVEL MINIMUM_DEVICE_LEVEL = D3D_FEATURE_LEVEL_12_0;

    ID3D11Device* initial_d3d11_device = NULL;
    ID3D11DeviceContext* initial_d3d11_context = NULL;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, &MINIMUM_DEVICE_LEVEL, 1, D3D11_SDK_VERSION, &initial_d3d11_device, NULL, &initial_d3d11_context);

    if (FAILED(hr))
    {
        error("ERROR: Could not create D3D11 device (%#x)\n", hr);
        goto rfail;
    }

    hr = initial_d3d11_device->QueryInterface(IID_PPV_ARGS(&vid_d3d11_device));

    if (FAILED(hr))
    {
        error("ERROR: Could not query for newer D3D11 device features (%#x)\n", hr);
        goto rfail;
    }

    hr = initial_d3d11_context->QueryInterface(IID_PPV_ARGS(&vid_d3d11_context));

    if (FAILED(hr))
    {
        error("ERROR: Could not query for newer D3D11 device context features (%#x)\n", hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    svr_maybe_release(&initial_d3d11_device);
    svr_maybe_release(&initial_d3d11_context);
    return ret;
}

bool EncoderState::vid_create_shaders()
{
    bool ret = false;

    vid_shader_mem = svr_alloc(VID_SHADER_SIZE);

    if (!vid_create_shader("convert_nv12", (void**)&vid_nv12_cs, D3D11_COMPUTE_SHADER))
    {
        goto rfail;
    }

    if (!vid_create_shader("convert_yuv422", (void**)&vid_yuv422_cs, D3D11_COMPUTE_SHADER))
    {
        goto rfail;
    }

    if (!vid_create_shader("convert_yuv444", (void**)&vid_yuv444_cs, D3D11_COMPUTE_SHADER))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    svr_free(vid_shader_mem);
    return ret;
}

void EncoderState::vid_free_static()
{
    svr_maybe_release(&vid_d3d11_device);
    svr_maybe_release(&vid_d3d11_context);

    svr_maybe_release(&vid_nv12_cs);
    svr_maybe_release(&vid_yuv422_cs);
    svr_maybe_release(&vid_yuv444_cs);
}

void EncoderState::vid_free_dynamic()
{
    svr_maybe_close_handle(&game_texture_h);

    svr_maybe_release(&vid_game_tex);
    svr_maybe_release(&vid_game_tex_srv);
    svr_maybe_release(&vid_game_tex_lock);

    for (s32 i = 0; i < VID_MAX_PLANES; i++)
    {
        svr_maybe_release(&vid_converted_texs[i]);
    }

    for (s32 i = 0; i < VID_QUEUED_TEXTURES; i++)
    {
        VidTextureDownloadInput* inp = &vid_texture_download_queue[i];

        for (s32 j = 0; j < VID_MAX_PLANES; j++)
        {
            svr_maybe_release(&inp->dl_texs[j]);
        }
    }

    vid_conversion_cs = NULL;
    vid_num_planes = 0;
}

bool EncoderState::vid_load_shader(const char* name)
{
    bool ret = false;

    HANDLE h = CreateFileA(svr_va("data\\shaders\\%s", name), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        error("Could not load shader %s (%lu)\n", name, GetLastError());
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

bool EncoderState::vid_create_shader(const char* name, void** shader, D3D11_SHADER_TYPE type)
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
        error("ERROR: Could not create shader %s (%#x)\n", name, hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::vid_start()
{
    bool ret = false;

    if (!vid_open_game_texture())
    {
        goto rfail;
    }

    vid_create_conversion_texs();

    render_download_write_idx = 0;
    render_download_read_idx = 0;

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool EncoderState::vid_open_game_texture()
{
    bool ret = false;
    HRESULT hr;

    hr = vid_d3d11_device->OpenSharedResource1((HANDLE)shared_mem_ptr->game_texture_h, IID_PPV_ARGS(&vid_game_tex));

    if (FAILED(hr))
    {
        error("ERROR: Could not open the shared svr_game texture (%#x)\n", hr);
        goto rfail;
    }

    vid_d3d11_device->CreateShaderResourceView(vid_game_tex, NULL, &vid_game_tex_srv);

    vid_game_tex->QueryInterface(IID_PPV_ARGS(&vid_game_tex_lock));

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

struct VidPlaneDesc
{
    DXGI_FORMAT format;
    s32 shift_x;
    s32 shift_y;
};

// Setup state and create the textures in the format that can be sent to the encoder.
void EncoderState::vid_create_conversion_texs()
{
    VidPlaneDesc plane_descs[VID_MAX_PLANES];

    switch (render_video_info->pixel_format)
    {
        case AV_PIX_FMT_NV12:
        {
            vid_conversion_cs = vid_nv12_cs;
            vid_num_planes = 2;

            plane_descs[0] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 0, 0 };
            plane_descs[1] = VidPlaneDesc { DXGI_FORMAT_R8G8_UINT, 1, 1 };
            break;
        }

        case AV_PIX_FMT_YUV422P:
        {
            vid_conversion_cs = vid_yuv422_cs;
            vid_num_planes = 3;

            plane_descs[0] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 0, 0 };
            plane_descs[1] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 1, 0 };
            plane_descs[2] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 1, 0 };
            break;
        }

        case AV_PIX_FMT_YUV444P:
        {
            vid_conversion_cs = vid_yuv444_cs;
            vid_num_planes = 3;

            plane_descs[0] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 0, 0 };
            plane_descs[1] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 0, 0 };
            plane_descs[2] = VidPlaneDesc { DXGI_FORMAT_R8_UINT, 0, 0 };
            break;
        }

        // This must work because the render info is our own thing.
        default: assert(false);
    }

    assert(vid_num_planes <= VID_MAX_PLANES);

    for (s32 i = 0; i < vid_num_planes; i++)
    {
        VidPlaneDesc* plane_desc = &plane_descs[i];

        D3D11_TEXTURE2D_DESC tex_desc = {};
        tex_desc.Width = movie_params.video_width >> plane_desc->shift_x;
        tex_desc.Height = movie_params.video_height >> plane_desc->shift_y;
        tex_desc.MipLevels = 1;
        tex_desc.ArraySize = 1;
        tex_desc.Format = plane_desc->format;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Usage = D3D11_USAGE_DEFAULT;
        tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
        tex_desc.CPUAccessFlags = 0;

        vid_plane_heights[i] = tex_desc.Height;

        vid_d3d11_device->CreateTexture2D(&tex_desc, NULL, &vid_converted_texs[i]);
        vid_d3d11_device->CreateUnorderedAccessView(vid_converted_texs[i], NULL, &vid_converted_uavs[i]);
    }

    for (s32 i = 0; i < VID_QUEUED_TEXTURES; i++)
    {
        VidTextureDownloadInput* input = &vid_texture_download_queue[i];

        for (s32 j = 0; j < vid_num_planes; j++)
        {
            ID3D11Texture2D* tex = vid_converted_texs[j];

            D3D11_TEXTURE2D_DESC tex_desc;
            tex->GetDesc(&tex_desc);

            // Also need to create an equivalent texture on the CPU side that we can copy into and then read from.

            tex_desc.Usage = D3D11_USAGE_STAGING;
            tex_desc.BindFlags = 0;
            tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            vid_d3d11_device->CreateTexture2D(&tex_desc, NULL, &input->dl_texs[j]);
        }
    }
}

// Convert pixel formats and push result to be retrieved later.
// This must be done to not stall too much.
void EncoderState::vid_push_texture_for_conversion()
{
    vid_game_tex_lock->AcquireSync(ENCODER_PROC_ID, INFINITE); // Allow us to read now.

    vid_d3d11_context->CSSetShader(vid_conversion_cs, NULL, 0);
    vid_d3d11_context->CSSetShaderResources(0, 1, &vid_game_tex_srv);
    vid_d3d11_context->CSSetUnorderedAccessViews(0, vid_num_planes, vid_converted_uavs, NULL);

    vid_d3d11_context->Dispatch(vid_get_num_cs_threads(movie_params.video_width), vid_get_num_cs_threads(movie_params.video_height), 1);

    vid_game_tex_lock->ReleaseSync(ENCODER_GAME_ID); // Give back to game.

    ID3D11ShaderResourceView* null_srv = NULL;
    ID3D11UnorderedAccessView* null_uav = NULL;

    vid_d3d11_context->CSSetShaderResources(0, 1, &null_srv);
    vid_d3d11_context->CSSetUnorderedAccessViews(0, 1, &null_uav, NULL);

    s64 wrapped_write_idx = render_download_write_idx & (VID_QUEUED_TEXTURES - 1);
    VidTextureDownloadInput* input = &vid_texture_download_queue[wrapped_write_idx];

    for (s32 i = 0; i < vid_num_planes; i++)
    {
        vid_d3d11_context->CopyResource(input->dl_texs[i], vid_converted_texs[i]);
    }

    // Must flush because we write to the same textures (vid_converted_texs) every time before copying.
    // If this is not done, the content of the texture would be overwritten and the last write would win.
    vid_d3d11_context->Flush();

    render_download_write_idx++;
}

// Download textures from graphics memory to system memory.
// At this point these textures are in the correct format ready for encoding.
// Polling through D3D11_MAP_FLAG_DO_NOT_WAIT is not useful in this case as we do not have any practical
// frame budget, as we process as fast as possible. This means that the writes would always be significantly ahead
// of the reads.
// Instead we just try and separate the writes from the reads through a large gap, in which hopefully the reads do not suffer too much slowdown.
// We always read from the oldest textures.
void EncoderState::vid_download_texture_into_frame(AVFrame* dest_frame)
{
    s64 wrapped_read_idx = render_download_read_idx & (VID_QUEUED_TEXTURES - 1);
    VidTextureDownloadInput* input = &vid_texture_download_queue[wrapped_read_idx];

    D3D11_MAPPED_SUBRESOURCE maps[VID_MAX_PLANES];

    for (s32 i = 0; i < vid_num_planes; i++)
    {
        vid_d3d11_context->Map(input->dl_texs[i], 0, D3D11_MAP_READ, 0, &maps[i]);
    }

    for (s32 i = 0; i < vid_num_planes; i++)
    {
        D3D11_MAPPED_SUBRESOURCE* map = &maps[i];
        s32 height = vid_plane_heights[i];
        u8* source_ptr = (u8*)map->pData;
        s32 source_line_size = map->RowPitch;
        u8* dest_ptr = dest_frame->data[i];
        s32 dest_line_size = dest_frame->linesize[i];

        for (s32 j = 0; j < height; j++)
        {
            memcpy(dest_ptr, source_ptr, dest_line_size);

            source_ptr += source_line_size;
            dest_ptr += dest_line_size;
        }
    }

    for (s32 i = 0; i < vid_num_planes; i++)
    {
        vid_d3d11_context->Unmap(input->dl_texs[i], 0);
    }

    render_download_read_idx++;
}

bool EncoderState::vid_can_map_now()
{
    s64 dist = render_download_write_idx - render_download_read_idx;
    return dist > VID_QUEUED_TEXTURES - 2;
}

bool EncoderState::vid_drain_textures()
{
    s64 dist = render_download_write_idx - render_download_read_idx;
    return dist > 0;
}

s32 EncoderState::vid_get_num_cs_threads(s32 unit)
{
    // Thread group divisor constant must match the thread count in the compute shaders!
    return svr_align32(unit, 8) >> 3;
}
