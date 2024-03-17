#include "proc_priv.h"

__declspec(align(16)) struct MosampleCb
{
    float mosample_weight;
};

bool ProcState::mosample_init()
{
    bool ret = false;

    if (!mosample_create_buffer())
    {
        goto rfail;
    }

    if (!mosample_create_shaders())
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool ProcState::mosample_create_buffer()
{
    bool ret = false;
    HRESULT hr;

    D3D11_BUFFER_DESC mosample_cb_desc = {};
    mosample_cb_desc.ByteWidth = sizeof(MosampleCb);
    mosample_cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    mosample_cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    mosample_cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    mosample_cb_desc.MiscFlags = 0;

    hr = vid_d3d11_device->CreateBuffer(&mosample_cb_desc, NULL, &mosample_cb);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create mosample constant buffer (%#x)\n", hr);
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool ProcState::mosample_create_shaders()
{
    bool ret = false;

    if (!vid_create_shader("mosample", (void**)&mosample_cs, D3D11_COMPUTE_SHADER))
    {
        goto rfail;
    }

    if (!vid_create_shader("downsample", (void**)&mosample_downsample_cs, D3D11_COMPUTE_SHADER))
    {
        goto rfail;
    }

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

bool ProcState::mosample_create_textures()
{
    bool ret = false;
    HRESULT hr;

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = movie_width;
    tex_desc.Height = movie_height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; // Must be high precision!
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET;

    hr = vid_d3d11_device->CreateTexture2D(&tex_desc, NULL, &mosample_work_tex);

    if (FAILED(hr))
    {
        svr_log("ERROR: Could not create work texture (%#x)\n", hr);
        goto rfail;
    }

    vid_d3d11_device->CreateShaderResourceView(mosample_work_tex, NULL, &mosample_work_tex_srv);
    vid_d3d11_device->CreateRenderTargetView(mosample_work_tex, NULL, &mosample_work_tex_rtv);
    vid_d3d11_device->CreateUnorderedAccessView(mosample_work_tex, NULL, &mosample_work_tex_uav);

    // The work texture may get reused by the runtime between renderings, so we must clear it.
    vid_clear_rtv(mosample_work_tex_rtv, 0.0f, 0.0f, 0.0f, 1.0f);

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void ProcState::mosample_free_static()
{
    svr_maybe_release(&mosample_work_tex);
    svr_maybe_release(&mosample_work_tex_rtv);
    svr_maybe_release(&mosample_work_tex_srv);
    svr_maybe_release(&mosample_work_tex_uav);

    svr_maybe_release(&mosample_cs);
    svr_maybe_release(&mosample_downsample_cs);
    svr_maybe_release(&mosample_cb);
}

void ProcState::mosample_free_dynamic()
{
}

bool ProcState::mosample_start()
{
    bool ret = false;

    if (!mosample_create_textures())
    {
        goto rfail;
    }

    mosample_remainder = 0.0f;

    s32 sps = movie_profile.video_fps * movie_profile.mosample_mult;
    mosample_remainder_step = (1.0f / sps) / (1.0f / movie_profile.video_fps);

    ret = true;
    goto rexit;

rfail:

rexit:
    return ret;
}

void ProcState::mosample_end()
{
}

// TODO Should be using VS/PS for this instead which is better suited for this kind of operation.
// Also probably consider to process several frames at once instead of just 1.
void ProcState::mosample_process(float weight)
{
    if (weight != mosample_weight_cache)
    {
        MosampleCb cb_data;
        cb_data.mosample_weight = weight;

        mosample_weight_cache = weight;
        vid_update_constant_buffer(mosample_cb, &cb_data, sizeof(MosampleCb));
    }

    vid_d3d11_context->CSSetShader(mosample_cs, NULL, 0);
    vid_d3d11_context->CSSetShaderResources(0, 1, &svr_game_texture.srv);
    vid_d3d11_context->CSSetConstantBuffers(0, 1, &mosample_cb);
    vid_d3d11_context->CSSetUnorderedAccessViews(0, 1, &mosample_work_tex_uav, NULL);

    vid_d3d11_context->Dispatch(vid_get_num_cs_threads(movie_width), vid_get_num_cs_threads(movie_height), 1);

    svr_start_prof(&mosample_prof);
    vid_d3d11_context->Flush();
    svr_end_prof(&mosample_prof);

    ID3D11ShaderResourceView* null_srv = NULL;
    ID3D11UnorderedAccessView* null_uav = NULL;

    vid_d3d11_context->CSSetShaderResources(0, 1, &null_srv);
    vid_d3d11_context->CSSetUnorderedAccessViews(0, 1, &null_uav, NULL);
}

void ProcState::mosample_new_video_frame()
{
    float old_rem = mosample_remainder;
    float exposure = movie_profile.mosample_exposure;

    mosample_remainder += mosample_remainder_step;

    if (mosample_remainder <= (1.0f - exposure))
    {
    }

    else if (mosample_remainder < 1.0f)
    {
        float weight = (mosample_remainder - svr_max(1.0f - exposure, old_rem)) * (1.0f / exposure);
        mosample_process(weight);
    }

    else
    {
        float weight = (1.0f - svr_max(1.0f - exposure, old_rem)) * (1.0f / exposure);

        mosample_process(weight);

        mosample_downsample_to_share_tex();

        process_finished_shared_tex();

        mosample_remainder -= 1.0f;

        s32 additional = mosample_remainder;

        if (additional > 0)
        {
            for (s32 i = 0; i < additional; i++)
            {
                process_finished_shared_tex();
            }

            mosample_remainder -= additional;
        }

        // Black is the only color that will work here, because the motion sampling is additive.
        vid_clear_rtv(mosample_work_tex_rtv, 0.0f, 0.0f, 0.0f, 1.0f);

        if (mosample_remainder > FLT_EPSILON && mosample_remainder > (1.0f - exposure))
        {
            weight = ((mosample_remainder - (1.0f - exposure)) * (1.0f / exposure));
            mosample_process(weight);
        }
    }
}

// Downsample 128 bpp texture to 32 bpp texture.
void ProcState::mosample_downsample_to_share_tex()
{
    vid_d3d11_context->CSSetShader(mosample_downsample_cs, NULL, 0);
    vid_d3d11_context->CSSetShaderResources(0, 1, &mosample_work_tex_srv);
    vid_d3d11_context->CSSetUnorderedAccessViews(0, 1, &encoder_share_tex_uav, NULL);

    vid_d3d11_context->Dispatch(vid_get_num_cs_threads(movie_width), vid_get_num_cs_threads(movie_height), 1);

    ID3D11ShaderResourceView* null_srv = NULL;
    ID3D11UnorderedAccessView* null_uav = NULL;

    vid_d3d11_context->CSSetShaderResources(0, 1, &null_srv);
    vid_d3d11_context->CSSetUnorderedAccessViews(0, 1, &null_uav, NULL);
}
