#include "game_priv.h"
#include "game_common.h"

// Convert sample format and send.
void game_prepare_and_send_sound_0(GameSndSample0* paint_buf, s32 num_samples)
{
    if (!svr_is_audio_enabled())
    {
        return;
    }

    SvrWaveSample* buf = (SvrWaveSample*)_alloca(sizeof(SvrWaveSample) * num_samples);

    for (s32 i = 0; i < num_samples; i++)
    {
        GameSndSample0* sample = &paint_buf[i];
        buf[i] = SvrWaveSample { (s16)sample->left, (s16)sample->right };
    }

    svr_give_audio(buf, num_samples);
}

// ----------------------------------------------------------------

void __cdecl game_snd_tx_stereo_override_0(void* unk, GameSndSample0* paint_buf, s32 paint_time, s32 end_time)
{
    if (!svr_movie_active())
    {
        return;
    }

    assert(game_state.snd_is_painting);

    s32 num_samples = end_time - paint_time;
    game_prepare_and_send_sound_0(paint_buf, num_samples);
}

// ----------------------------------------------------------------

void __fastcall game_snd_device_tx_samples_override_0(void* p, void* edx, u32 unused)
{
    if (!svr_movie_active())
    {
        // For this override, we must call the original in case we are not recording.
        // Otherwise there will only be sound when recording.

        using OrgFn = decltype(game_snd_device_tx_samples_override_0)*;
        OrgFn org_fn = (OrgFn)game_state.snd_device_tx_samples_hook.original;
        org_fn(p, edx, unused);
        return;
    }

    assert(game_state.snd_is_painting);
    assert(game_state.snd_num_samples);

    GameSndSample0* paint_buf = game_get_snd_paint_buffer_0();
    game_prepare_and_send_sound_0(paint_buf, game_state.snd_num_samples);
}

// ----------------------------------------------------------------

void __cdecl game_snd_paint_chans_override_0(s32 end_time, bool is_underwater)
{
    game_state.snd_listener_underwater = is_underwater;

    if (svr_movie_active() && !game_state.snd_is_painting)
    {
        return; // When movie is active we call this ourselves with the real number of samples write.
    }

    // Will call game_snd_tx_stereo_override_0 or game_snd_device_tx_samples_override_0.

    using OrgFn = decltype(game_snd_paint_chans_override_0)*;
    OrgFn org_fn = (OrgFn)game_state.snd_paint_chans_hook.original;
    org_fn(end_time, is_underwater);
}

void __cdecl game_snd_paint_chans_override_1(s64 end_time, bool is_underwater)
{
    game_state.snd_listener_underwater = is_underwater;

    if (svr_movie_active() && !game_state.snd_is_painting)
    {
        return; // When movie is active we call this ourselves with the real number of samples write.
    }

    // Will call game_snd_tx_stereo_override_0 or game_snd_device_tx_samples_override_0.

    using OrgFn = decltype(game_snd_paint_chans_override_1)*;
    OrgFn org_fn = (OrgFn)game_state.snd_paint_chans_hook.original;
    org_fn(end_time, is_underwater);
}

// ----------------------------------------------------------------

void __cdecl game_start_movie_override_0(void* cmd_args)
{
    game_rec_start_movie(cmd_args);
}

// ----------------------------------------------------------------

void __cdecl game_end_movie_override_0(void* cmd_args)
{
    game_rec_end_movie();
}

// ----------------------------------------------------------------

bool __fastcall game_eng_filter_time_override_0(void* p, void* edx, float dt)
{
    bool ret = game_rec_run_frame();

    if (!ret)
    {
        using OrgFn = decltype(game_eng_filter_time_override_0)*;
        OrgFn org_fn = (OrgFn)game_state.filter_time_hook.original;
        ret = org_fn(p, edx, dt);
    }

    return ret;
}

#ifndef _WIN64
bool __fastcall game_eng_filter_time_override_1(void* p, void* edx)
{
    float dt;

    __asm movss dt, xmm1;

    bool ret = game_rec_run_frame();

    if (!ret)
    {
        __asm movss xmm1, dt;

        using OrgFn = decltype(game_eng_filter_time_override_1)*;
        OrgFn org_fn = (OrgFn)game_state.filter_time_hook.original;
        ret = org_fn(p, edx);
    }

    return ret;
}
#endif

// ----------------------------------------------------------------

float __cdecl game_adjust_interpolation_amount_override_0(void* p, float base_interp)
{
    if (svr_movie_active())
    {
        return (1.0f / svr_get_video_frame_rate()) * 2.0f;
    }

    using OrgFn = decltype(game_adjust_interpolation_amount_override_0)*;
    OrgFn org_fn = (OrgFn)game_state.adjust_interpolation_amount_hook.original;

    float ret = org_fn(p, base_interp);
    return ret;
}

// ----------------------------------------------------------------

// Ray_t, trace_t and ITraceFilter for tracing.

struct alignas(16) GameTraceVector
{
    float x;
    float y;
    float z;
    float w;
};

struct GameTraceRay
{
    GameTraceVector start;
    GameTraceVector delta;
    GameTraceVector start_offset;
    GameTraceVector extents;
    bool is_ray;
    bool is_swept;
};

struct GameTraceResult
{
    float start_pos[3];
    float end_pos[3];
    float plane_normal[3];
    float plane_dist;
    u8 plane_type;
    u8 plane_signbits;
    u8 plane_pad[2];
    float fraction;
    s32 contents;
    u16 disp_flags;
    bool all_solid;
    bool start_solid;
    float fraction_left_solid;
    const char* surface_name;
    s16 surface_props;
    u16 surface_flags;
    s32 hit_group;
    s16 physics_bone;
    void* entity;
    s32 hitbox;
};

static_assert(sizeof(GameTraceRay) == 80);
static_assert(offsetof(GameTraceResult, fraction) == 44);
static_assert(sizeof(GameTraceResult) == 104);

// Only hits the world.
struct GameTraceWorldFilter
{
    virtual bool should_hit_entity(void* entity, s32 contents_mask) { return false; }
    virtual s32 get_trace_type() { return 1; }
};

// The eye is 2 units above the top of the hull, so the view goes into the ceiling when the player hits their head.
// Keep the view 12 units under anything above it.
void __cdecl game_override_view_override_0(void* p, void* view_setup)
{
    using OrgFn = decltype(game_override_view_override_0)*;
    OrgFn org_fn = (OrgFn)game_state.override_view_hook.original;
    org_fn(p, view_setup);

    if (!svr_movie_active())
    {
        return;
    }

    float* origin = (float*)((u8*)view_setup + 64);

    // Trace a flat box from the top of the hull to 12 units above the eye.
    // It's 1 unit narrower than the hull so walls next to the player don't hit.
    GameTraceRay ray = {};
    ray.start = { origin[0], origin[1], origin[2] - 2.0f, 0.0f };
    ray.delta = { 0.0f, 0.0f, 14.0f, 0.0f };
    ray.extents = { 15.0f, 15.0f, 0.0f, 0.0f };
    ray.is_ray = false;
    ray.is_swept = true;

    GameTraceResult tr = {};
    GameTraceWorldFilter filter;

    // MASK_SOLID_BRUSHONLY.
    using TraceRayFn = void(__cdecl*)(void* p, GameTraceRay* ray, u32 mask, GameTraceWorldFilter* filter, GameTraceResult* tr);
    TraceRayFn trace_ray_fn = (TraceRayFn)game_get_virtual(game_state.search_desc.engine_trace, 4);
    trace_ray_fn(game_state.search_desc.engine_trace, &ray, 0x400B, &filter, &tr);

    if (!(tr.fraction >= 0.0f && tr.fraction < 1.0f))
    {
        return;
    }

    origin[2] = ray.start.z + tr.fraction * ray.delta.z - 12.0f;
}

// ----------------------------------------------------------------

void game_overrides_init()
{
    game_hook_create(&game_state.search_desc.start_movie_override, &game_state.start_movie_hook);
    game_hook_create(&game_state.search_desc.end_movie_override, &game_state.end_movie_hook);
    game_hook_create(&game_state.search_desc.filter_time_override, &game_state.filter_time_hook);

    if (game_state.search_desc.caps & GAME_CAP_HAS_AUDIO)
    {
        game_hook_create(&game_state.search_desc.snd_paint_chans_override, &game_state.snd_paint_chans_hook);
    }

    if (game_state.search_desc.caps & GAME_CAP_AUDIO_DEVICE_1)
    {
        game_hook_create(&game_state.search_desc.snd_tx_stereo_override, &game_state.snd_tx_stereo_hook);
    }

    if (game_state.search_desc.caps & (GAME_CAP_AUDIO_DEVICE_1_5 | GAME_CAP_AUDIO_DEVICE_2))
    {
        game_hook_create(&game_state.search_desc.snd_device_tx_samples_override, &game_state.snd_device_tx_samples_hook);
    }

    if (game_state.search_desc.caps & GAME_CAP_HAS_LAGCOMP)
    {
        // Too inconsistent.
        // game_hook_create(&game_state.search_desc.adjust_interpolation_amount_override, &game_state.adjust_interpolation_amount_hook);
    }

    if (game_state.search_desc.caps & GAME_CAP_HAS_CEILING_CLEARANCE)
    {
        game_hook_create(&game_state.search_desc.override_view_override, &game_state.override_view_hook);
    }
}
