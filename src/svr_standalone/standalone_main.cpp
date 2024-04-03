#include "standalone_priv.h"

// Entrypoint for standalone SVR. Reverse engineered code to use the SVR API from unsupported games.
//
// This hooks the following locations in the game code:
// *) startmovie console command and reads its arguments.
// *) endmovie console command to end movie processing.
// *) Engine frame function to do frame processing.
//
// Here we can also submit console commands to set some console variables to their optimal values during movie processing.
// Unfortunately we cannot reset these values so we have to rely on setting them through svr_movie_start.cfg and svr_movie_end.cfg.
// That is a good function because it allows users to put more stuff in there that they want to set and restore.

// We don't have to verify that we actually are the game that we get passed in, as it is impossible to fabricate
// these variables externally without modifying the launcher code.
// If our code even runs in here then the game already exists and we don't have to deal with weird cases.

// Stuff passed in from launcher.
SvrGameInitData launcher_data;
DWORD main_thread_id;

// --------------------------------------------------------------------------

BOOL CALLBACK EnumThreadWndProc(HWND hwnd, LPARAM lParam)
{
    HWND* out_hwnd = (HWND*)lParam;
    *out_hwnd = hwnd;
    return FALSE;
}

void standalone_error(const char* format, ...)
{
    char message[1024];

    va_list va;
    va_start(va, format);
    SVR_VSNPRINTF(message, format, va);
    va_end(va);

    svr_log("!!! ERROR: %s\n", message);

    // MB_TASKMODAL or MB_APPLMODAL flags do not work.

    HWND hwnd = NULL;
    EnumThreadWindows(main_thread_id, EnumThreadWndProc, (LPARAM)&hwnd);

    MessageBoxA(hwnd, message, "SVR", MB_ICONERROR | MB_OK);

    ExitProcess(1);
}

// --------------------------------------------------------------------------

s32 query_proc_modules(HANDLE proc, HMODULE* list, s32 size)
{
    DWORD required_bytes;
    EnumProcessModules(proc, list, size * sizeof(HMODULE), &required_bytes);

    return required_bytes / sizeof(HMODULE);
}

void get_nice_module_name(HANDLE proc, HMODULE mod, char* buf, s32 size)
{
    // This buffer may contain the absolute path, we only want the name and extension.

    char temp[MAX_PATH];
    GetModuleFileNameExA(proc, (HMODULE)mod, temp, MAX_PATH);

    char* name = PathFindFileNameA(temp);

    StringCchCopyA(buf, size, name);
}

s32 check_loaded_proc_modules(const char** list, s32 size)
{
    const s32 NUM_MODULES = 384;

    // Use a large array here.
    // Some games like CSGO use a lot of libraries.

    HMODULE module_list[NUM_MODULES];

    HANDLE proc = GetCurrentProcess();

    s32 module_list_size = query_proc_modules(proc, module_list, NUM_MODULES);
    s32 hits = 0;

    // See if any of the requested modules are loaded.

    for (s32 i = 0; i < module_list_size; i++)
    {
        char name[MAX_PATH];
        get_nice_module_name(proc, module_list[i], name, MAX_PATH);

        for (s32 j = 0; j < size; j++)
        {
            if (!strcmpi(list[j], name))
            {
                hits++;
                break;
            }
        }

        if (hits == size)
        {
            break;
        }
    }

    return hits;
}

struct FnHook
{
    void* target;
    void* hook;
    void* original;
};

struct FnOverride
{
    void* target;
    void* hook;
};

bool wait_for_libs_to_load(const char** libs, s32 num)
{
    // Alternate method instead of hooking the LoadLibrary family of functions.
    // We don't need particular accuracy so this is good enough and much simpler.

    for (s32 i = 0; i < 60; i++)
    {
        if (check_loaded_proc_modules(libs, num) == num)
        {
            return true;
        }

        Sleep(500);
    }

    return false;
}

void hook_function(FnOverride override, FnHook* result_hook)
{
    // Either this works or the process crashes, so no point handling errors.
    // The return value of MH_CreateHook is not useful, because a non executable page for example is not a useful error,
    // as a mismatch in the pattern can still point to an incorrect executable page.

    void* orig = NULL;

    MH_CreateHook(override.target, override.hook, &orig);

    result_hook->target = override.target;
    result_hook->hook = override.hook;
    result_hook->original = orig;
}

// How many bytes there can be in a pattern scan.
const s32 MAX_SCAN_BYTES = 256;

struct ScanPattern
{
    // Negative value means unknown byte.
    s16 bytes[MAX_SCAN_BYTES];
    s16 used = 0;
};

bool is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

void pattern_bytes_from_string(const char* input, ScanPattern* out)
{
    const char* ptr = input;

    for (; *ptr != 0; ptr++)
    {
        assert(out->used < MAX_SCAN_BYTES);

        if (is_hex_char(*ptr))
        {
            assert(is_hex_char(*(ptr + 1)));

            out->bytes[out->used] = strtol(ptr, NULL, 16);
            out->used++;
            ptr++;
        }

        else if (*ptr == '?')
        {
            assert(*(ptr + 1) == '?');

            out->bytes[out->used] = -1;
            out->used++;
            ptr++;
        }
    }

    assert(out->used > 0);
}

bool compare_data(u8* data, ScanPattern* pattern)
{
    s32 index = 0;

    s16* bytes = pattern->bytes;

    for (s32 i = 0; i < pattern->used; i++)
    {
        s16 byte = *bytes;

        if (byte > -1 && *data != byte)
        {
            return false;
        }

        ++data;
        ++bytes;
        ++index;
    }

    return index == pattern->used;
}

void* find_pattern(void* start, s32 search_length, ScanPattern* pattern)
{
    s16 length = pattern->used;

    for (s32 i = 0; i <= search_length - length; ++i)
    {
        u8* addr = (u8*)start + i;

        if (compare_data(addr, pattern))
        {
            return addr;
        }
    }

    return NULL;
}

// A pattern scan with no match will result in NULL.
void verify_pattern_scan(void* addr, const char* name)
{
    if (addr == NULL)
    {
        svr_log("Pattern %s had no match\n", name);
        standalone_error("Mismatch between game version and supported SVR version. Ensure you are using the latest version of SVR and upload your SVR_LOG.txt.");
    }
}

void* pattern_scan(const char* module, const char* pattern, const char* name)
{
    MODULEINFO info;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(module), &info, sizeof(MODULEINFO));

    ScanPattern pattern_bytes;
    pattern_bytes_from_string(pattern, &pattern_bytes);

    void* ret = find_pattern(info.lpBaseOfDll, info.SizeOfImage, &pattern_bytes);
    verify_pattern_scan(ret, name);
    return ret;
}

void apply_patch(void* target, u8* bytes, s32 num_bytes)
{
    // Make page writable.

    DWORD old_protect;
    VirtualProtect(target, num_bytes, PAGE_EXECUTE_READWRITE, &old_protect);

    memcpy(target, bytes, num_bytes);

    VirtualProtect(target, num_bytes, old_protect, NULL);
}

void* create_interface(const char* module, const char* name)
{
    using CreateInterfaceFn = void*(__cdecl*)(const char* name, s32* code);

    HMODULE hmodule = GetModuleHandleA(module);
    CreateInterfaceFn fn = (CreateInterfaceFn)GetProcAddress(hmodule, "CreateInterface");

    s32 code;
    return fn(name, &code);
}

void* get_virtual(void* ptr, s32 index)
{
    void** vtable = *((void***)ptr);
    return vtable[index];
}

void* get_export(const char* module, const char* name)
{
    HMODULE hmodule = GetModuleHandleA(module);
    return GetProcAddress(hmodule, name);
}

// --------------------------------------------------------------------------

struct GmSndSample
{
    s32 left;
    s32 right;
};

enum
{
    RECORD_STATE_STOPPED,
    RECORD_STATE_WAITING,
    RECORD_STATE_POSSIBLE,
};

IDirect3DDevice9Ex* gm_d3d9ex_device;
void* gm_engine_client_ptr;

void* gm_engine_client_exec_cmd_fn;
void* gm_signon_state_ptr;

void* gm_local_player_ptr;
void* gm_get_spec_target_fn;
void* gm_get_player_by_index_fn;
void* gm_snd_paint_time_ptr;
void* gm_local_or_spec_target_fn;

FnHook start_movie_hook;
FnHook end_movie_hook;
FnHook eng_filter_time_hook;

FnHook snd_tx_stereo_hook;
FnHook snd_mix_chans_hook;
FnHook snd_device_tx_hook;

FnHook d3d9ex_present_hook;
FnHook d3d9ex_present_ex_hook;

bool disable_window_update;

bool snd_is_painting;
bool snd_listener_underwater;

void* gm_snd_paint_buffer;

// The number of samples to submit must align to 4 sample boundaries, that means there may be samples over
// that we have to process in the next frame.
s32 snd_skipped_samples;

// Time that was lost between the fps to sample rate conversion. This is added back next frame.
float snd_lost_mix_time;

s32 snd_num_samples;

s64 tm_num_frames;
s64 tm_first_frame_time;
s64 tm_last_frame_time;

s32 recording_state;

bool enable_autostop;

// Velo is restricted to the movement based games.
// This is done in order to increase reach for games, such as Source 2013 mods which may modify client.dll (otherwise every mod may need tailored setup).
// We assume that those games only want the capture part of SVR.
bool can_use_velo()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            return true;
        }
    }

    return false;
}

void client_command(const char* cmd)
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            using GmClientCmdFn = void(__fastcall*)(void* p, void* edx, const char* str);
            GmClientCmdFn fn = (GmClientCmdFn)gm_engine_client_exec_cmd_fn;
            fn(gm_engine_client_ptr, NULL, cmd);
            break;
        }

        default: assert(false);
    }
}

s32 get_spec_target()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            using GmGetSpecTargetFn = s32(__cdecl*)();
            GmGetSpecTargetFn fn = (GmGetSpecTargetFn)gm_get_spec_target_fn;
            return fn();
        }

        default: assert(false);
    }

    return 0;
}

void* get_player_by_idx(s32 index)
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            using GmPlayerByIdxFn = void*(__cdecl*)(s32 index);
            GmPlayerByIdxFn fn = (GmPlayerByIdxFn)gm_get_player_by_index_fn;
            return fn(index);
        }

        default: assert(false);
    }

    return NULL;
}

void* get_local_player()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            return **(void***)gm_local_player_ptr;
        }

        default: assert(false);
    }

    return NULL;
}

// Return local player or spectated player (for mp games).
void* get_active_player()
{
    void* player = NULL;

    if (launcher_data.app_id == STEAM_GAME_CSGO)
    {
        using GmLocalOrSpecTargetFn = void*(__cdecl*)();
        GmLocalOrSpecTargetFn fn = (GmLocalOrSpecTargetFn)gm_local_or_spec_target_fn;
        player = fn();
    }

    else
    {
        // There is a bug here if you go from a demo with bots to a local game with bots,
        // where the sticky indexes are still valid so we end up not reading from the local player.
        // Not sure how to reset the spec target easily without introducing yet more patterns.

        s32 spec = get_spec_target();

        if (spec > 0)
        {
            player = get_player_by_idx(spec);
        }

        // It's possible to be spectating someone without that player having any entity.
        if (player == NULL)
        {
            player = get_local_player();
        }
    }

    return player;
}

// Return the absolute velocity variable.
float* get_player_vel(void* player)
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        {
            u8* addr = (u8*)player + 244;
            return (float*)addr;
        }

        case STEAM_GAME_CSGO:
        {
            u8* addr = (u8*)player + 276;
            return (float*)addr;
        }

        case STEAM_GAME_TF2:
        {
            u8* addr = (u8*)player + 348;
            return (float*)addr;
        }

        default: assert(false);
    }

    return NULL;
}

void __cdecl snd_paint_chans_override(s32 end_time, bool is_underwater)
{
    snd_listener_underwater = is_underwater;

    if (svr_movie_active() && !snd_is_painting)
    {
        // When movie is active we call this ourselves with the real number of samples write.
        return;
    }

    // Will call snd_tx_stereo_override or snd_device_tx_override.

    using GmSndPaintChansFn = void(__cdecl*)(s32 end_time, bool is_underwater);
    GmSndPaintChansFn org_fn = (GmSndPaintChansFn)snd_mix_chans_hook.original;
    org_fn(end_time, is_underwater);
}

void __cdecl snd_paint_chans_override2(s64 end_time, bool is_underwater)
{
    snd_listener_underwater = is_underwater;

    if (svr_movie_active() && !snd_is_painting)
    {
        // When movie is active we call this ourselves with the real number of samples write.
        return;
    }

    // Will call snd_tx_stereo_override or snd_device_tx_override.

    using GmSndPaintChansFn2 = void(__cdecl*)(s64 end_time, bool is_underwater);
    GmSndPaintChansFn2 org_fn = (GmSndPaintChansFn2)snd_mix_chans_hook.original;
    org_fn(end_time, is_underwater);
}

void prepare_and_send_sound(GmSndSample* paint_buf, s32 num_samples)
{
    if (!svr_is_audio_enabled())
    {
        return;
    }

    SvrWaveSample* buf = (SvrWaveSample*)_alloca(sizeof(SvrWaveSample) * num_samples);

    for (s32 i = 0; i < num_samples; i++)
    {
        GmSndSample* sample = &paint_buf[i];
        buf[i] = SvrWaveSample { (s16)sample->left, (s16)sample->right };
    }

    svr_give_audio(buf, num_samples);
}

void __cdecl snd_tx_stereo_override(void* unk, GmSndSample* paint_buf, s32 paint_time, s32 end_time)
{
    if (!svr_movie_active())
    {
        return;
    }

    assert(snd_is_painting);

    s32 num_samples = end_time - paint_time;
    prepare_and_send_sound(paint_buf, num_samples);
}

void __fastcall snd_device_tx_override(void* p, void* edx, u32 unused)
{
    if (!svr_movie_active())
    {
        return;
    }

    assert(snd_is_painting);
    assert(snd_num_samples);

    GmSndSample* paint_buf = **(GmSndSample***)gm_snd_paint_buffer;
    prepare_and_send_sound(paint_buf, snd_num_samples);
}

void give_player_velo()
{
    void* player = get_active_player();

    if (player)
    {
        svr_give_velocity(get_player_vel(player));
    }
}

// The DirectSound backend need times to be aligned to 4 sample boundaries.
// We round down because we cannot add more samples out of thin air. The skipped samples are remembered for the next iteration.
// This happens to also work for CSGO that doesn't use DirectSound.
s32 align_sample_time(s32 value)
{
    return value & ~3;
}

void mix_audio_for_one_frame()
{
    // Figure out how many samples we need to process for this frame.

    s32 paint_time = **(s32**)gm_snd_paint_time_ptr;

    float time_ahead_to_mix = 1.0f / (float)svr_get_game_rate();
    float num_frac_samples_to_mix = (time_ahead_to_mix * 44100.0f) + snd_lost_mix_time;

    s32 num_samples_to_mix = (s32)num_frac_samples_to_mix;
    snd_lost_mix_time = num_frac_samples_to_mix - (float)num_samples_to_mix;

    s32 raw_end_time = paint_time + num_samples_to_mix + snd_skipped_samples;
    s32 aligned_end_time = align_sample_time(raw_end_time);

    s32 num_samples = aligned_end_time - paint_time;

    snd_skipped_samples = raw_end_time - aligned_end_time;

    if (num_samples > 0)
    {
        snd_is_painting = true;
        snd_paint_chans_override(aligned_end_time, snd_listener_underwater);
        snd_is_painting = false;
    }
}

// Almost duplicate but whatever, CSGO sound is enough different to warrant.
void mix_audio_for_one_frame2()
{
    // Figure out how many samples we need to process for this frame.

    s64 paint_time = **(s64**)gm_snd_paint_time_ptr;

    float time_ahead_to_mix = 1.0f / (float)svr_get_game_rate();
    float num_frac_samples_to_mix = (time_ahead_to_mix * 44100.0f) + snd_lost_mix_time;

    s64 num_samples_to_mix = (s64)num_frac_samples_to_mix;
    snd_lost_mix_time = num_frac_samples_to_mix - (float)num_samples_to_mix;

    s64 raw_end_time = paint_time + num_samples_to_mix + snd_skipped_samples;
    s64 aligned_end_time = align_sample_time(raw_end_time);

    s64 num_samples = aligned_end_time - paint_time;

    snd_skipped_samples = raw_end_time - aligned_end_time;

    if (num_samples > 0)
    {
        // For CSGO we need to store the number of samples to process.
        snd_num_samples = num_samples;

        snd_is_painting = true;
        snd_paint_chans_override2(aligned_end_time, snd_listener_underwater);
        snd_is_painting = false;
    }
}

void do_recording_frame()
{
    if (tm_num_frames == 0)
    {
        tm_first_frame_time = svr_prof_get_real_time();
    }

    if (launcher_data.app_id == STEAM_GAME_CSGO)
    {
        mix_audio_for_one_frame2();
    }

    else
    {
        mix_audio_for_one_frame();
    }

    if (svr_is_velo_enabled() && can_use_velo())
    {
        give_player_velo();
    }

    svr_frame();

    tm_num_frames++;
}

// Recording should only be possible when fully connected. We don't want the menu and loading screen to be recorded.
// When we start the movie we set the recording state to waiting, which means waiting for the connection to finish to where we are in game fully.
void update_recording_state()
{
    const s32 SIGNON_STATE_NONE = 0;
    const s32 SIGNON_STATE_FULL = 6;

    s32 state = -1;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            state = **(s32**)gm_signon_state_ptr;
            break;
        }

        case STEAM_GAME_CSGO:
        {
            state = *(s32*)gm_signon_state_ptr;
            break;
        }

        default: assert(false);
    }

    if (state == SIGNON_STATE_NONE)
    {
        // Autostop.
        if (recording_state == RECORD_STATE_POSSIBLE)
        {
            if (enable_autostop)
            {
                recording_state = RECORD_STATE_STOPPED;
            }

            else
            {
                // Wait until we connect again.
                recording_state = RECORD_STATE_WAITING;
            }
        }
    }

    else if (state == SIGNON_STATE_FULL)
    {
        // Autostart.
        if (recording_state == RECORD_STATE_WAITING)
        {
            recording_state = RECORD_STATE_POSSIBLE;
        }
    }
}

void check_autostop()
{
    // If we disconnected the previous frame, stop recording this frame.
    if (recording_state == RECORD_STATE_STOPPED && svr_movie_active())
    {
        void end_the_movie();
        end_the_movie();
    }
}

// If we are recording this frame then don't filter time.
bool run_frame()
{
    update_recording_state();
    check_autostop();

    if (recording_state == RECORD_STATE_POSSIBLE && svr_movie_active())
    {
        do_recording_frame();
        return true;
    }

    return false;
}

bool __fastcall eng_filter_time_override(void* p, void* edx, float dt)
{
    bool ret = run_frame();

    if (!ret)
    {
        using GmEngFilterTimeFn = bool(__fastcall*)(void* p, void* edx, float dt);
        GmEngFilterTimeFn org_fn = (GmEngFilterTimeFn)eng_filter_time_hook.original;
        ret = org_fn(p, edx, dt);
    }

    return ret;
}

bool __fastcall eng_filter_time_override2(void* p, void* edx)
{
    float dt;

    __asm {
        movss dt, xmm1
    };

    bool ret = run_frame();

    if (!ret)
    {
        __asm {
            movss xmm1, dt
        };

        using GmEngFilterTimeFn2 = bool(__fastcall*)(void* p, void* edx);
        GmEngFilterTimeFn2 org_fn = (GmEngFilterTimeFn2)eng_filter_time_hook.original;
        ret = org_fn(p, edx);
    }

    return ret;
}

HRESULT __stdcall d3d9ex_present_override(void* p, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
{
    if (svr_movie_active())
    {
        return S_OK;
    }

    using OrgFn = decltype(d3d9ex_present_override)*;
    OrgFn org_fn = (OrgFn)d3d9ex_present_hook.original;
    return org_fn(p, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall d3d9ex_present_ex_override(void* p, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags)
{
    if (svr_movie_active())
    {
        return S_OK;
    }

    using OrgFn = decltype(d3d9ex_present_ex_override)*;
    OrgFn org_fn = (OrgFn)d3d9ex_present_ex_hook.original;
    return org_fn(p, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

// Subsitute for running exec on a cfg in the game directory. This is for running a cfg in the SVR directory.
bool run_cfg(const char* name)
{
    char full_cfg_path[MAX_PATH];
    full_cfg_path[0] = 0;
    StringCchCatA(full_cfg_path, MAX_PATH, launcher_data.svr_path);
    StringCchCatA(full_cfg_path, MAX_PATH, "\\data\\cfg\\");
    StringCchCatA(full_cfg_path, MAX_PATH, name);

    HANDLE h = INVALID_HANDLE_VALUE;
    char* mem = NULL;
    bool ret = false;

    h = CreateFileA(full_cfg_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
        {
            svr_log("Could not open cfg %s (%lu)\n", full_cfg_path, GetLastError());
        }

        goto rfail;
    }

    LARGE_INTEGER file_size;
    GetFileSizeEx(h, &file_size);

    // There are 2 points for this case. The process may run out of memory even if the file size is way less than this (can't control that).
    // But the more important thing is to not overflow the LowPart below when we add more bytes for the extra characters we need to submit to the console.
    if (file_size.LowPart > MAXDWORD - 2)
    {
        svr_log("Refusing to open cfg %s as it is too big\n", full_cfg_path);
        goto rfail;
    }

    // Commands must end with a newline, also need to terminate.

    mem = (char*)malloc(file_size.LowPart + 2);
    ReadFile(h, mem, file_size.LowPart, NULL, NULL);
    mem[file_size.LowPart - 1] = '\n';
    mem[file_size.LowPart] = 0;

    svr_log("Running cfg %s\n", name);

    // The file can be executed as is. The game takes care of splitting by newline.
    // We don't monitor what is inside the cfg, it's up to the user.
    client_command(mem);

    ret = true;
    goto rexit;

rfail:
rexit:
    if (mem) free(mem);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    return ret;
}

// Run all user cfgs for a given event (such as movie start or movie end).
void run_user_cfgs_for_event(const char* name)
{
    run_cfg(svr_va("svr_movie_%s_user.cfg", name));
    run_cfg(svr_va("svr_movie_%s_%u.cfg", name, launcher_data.app_id));
}

// Return the full console command args string.
const char* get_cmd_args(void* args)
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            u8* addr = (u8*)args + 8;
            return (const char*)addr;
        }

        default: assert(false);
    }

    return NULL;
}

void __cdecl start_movie_override(void* args)
{
    tm_num_frames = 0;
    tm_first_frame_time = 0;
    tm_last_frame_time = 0;

    snd_skipped_samples = 0;
    snd_lost_mix_time = 0.0f;
    snd_num_samples = 0;

    // We don't want to call the original function for this command.

    if (svr_movie_active())
    {
        game_console_msg("Movie already started\n");
        return;
    }

    const char* value_args = get_cmd_args(args);

    // First argument is always startmovie.

    char movie_name[MAX_PATH];
    movie_name[0] = 0;

    char profile[MAX_PATH];
    profile[0] = 0;

    s32 used_args = sscanf_s(value_args, "%*s %s %s", movie_name, MAX_PATH - 5, profile, MAX_PATH - 5);

    if (used_args == 0 || used_args == EOF)
    {
        game_console_msg("Usage: startmovie <name> (<optional profile>)\n");
        game_console_msg("Starts to record a movie with an optional profile\n");
        game_console_msg("For more information see https://github.com/crashfort/SourceDemoRender\n");
        return;
    }

    // Will point to the end if no extension was provided.
    const char* movie_ext = PathFindExtensionA(movie_name);

    if (*movie_ext == 0)
    {
        StringCchCatA(movie_name, MAX_PATH, ".mp4");
    }

    else
    {
        // Only allowed containers that have sufficient encoder support.
        bool has_valid_name = !strcmpi(movie_ext, ".mp4") || !strcmpi(movie_ext, ".mkv") || !strcmpi(movie_ext, ".mov");

        if (!has_valid_name)
        {
            char renamed[MAX_PATH];
            StringCchCopyA(renamed, MAX_PATH, movie_name);
            PathRenameExtensionA(renamed, ".mp4");

            StringCchCopyA(movie_name, MAX_PATH, renamed);
        }
    }

    IDirect3DSurface9* bb_surf = NULL;

    // Some commands must be set before svr_start (such as mat_queue_mode 0, due to the backbuffer ordering of gm_d3d9ex_device->GetRenderTarget call).
    // This file must always be run! Movie cannot be started otherwise!
    if (!run_cfg("svr_movie_start.cfg"))
    {
        game_log("Required cfg svr_start_movie.cfg could not be run\n");
        goto rfail;
    }

    run_user_cfgs_for_event("start");

    // The game backbuffer is the first index.
    gm_d3d9ex_device->GetRenderTarget(0, &bb_surf);

    // Audio parameters are fixed in Source so they cannot be changed, just better to have those constants in here.

    SvrStartMovieData startmovie_data;
    startmovie_data.game_tex_view = bb_surf;
    startmovie_data.audio_params.audio_channels = 2;
    startmovie_data.audio_params.audio_hz = 44100;
    startmovie_data.audio_params.audio_bits = 16;

    if (!svr_start(movie_name, profile, &startmovie_data))
    {
        // Reverse above changes if something went wrong.
        run_cfg("svr_movie_end.cfg");
        run_user_cfgs_for_event("end");
        goto rfail;
    }

    // Ensure the game runs at a fixed rate.

    char hfr_buf[32];
    StringCchPrintfA(hfr_buf, 32, "host_framerate %d\n", svr_get_game_rate());

    client_command(hfr_buf);

    // Allow recording the next frame.
    recording_state = RECORD_STATE_WAITING;

    game_log("Starting movie to %s\n", movie_name);

    goto rexit;

rfail:
    game_console_msg("Movie could not be started\n");

rexit:
    svr_maybe_release(&bb_surf);
}

void end_the_movie()
{
    assert(svr_movie_active());

    recording_state = RECORD_STATE_STOPPED;
    tm_last_frame_time = svr_prof_get_real_time();

    float time_taken = 0.0f;
    float fps = 0.0f;

    if (tm_last_frame_time > 0 && tm_first_frame_time > 0)
    {
        time_taken = (tm_last_frame_time - tm_first_frame_time) / 1000000.0f;
        fps = (float)tm_num_frames / time_taken;
    }

    game_log("Ending movie after %0.2f seconds (%lld frames, %0.2f fps)\n", time_taken, tm_num_frames, fps);

    svr_stop();

    run_cfg("svr_movie_end.cfg");
    run_user_cfgs_for_event("end");
}

void __cdecl end_movie_override(void* args)
{
    // We don't want to call the original function for this command.

    if (!svr_movie_active())
    {
        game_console_msg("Movie not started\n");
        return;
    }

    end_the_movie();
}

// If some game does not use the default convention then add a new array for that.

const char* COMMON_LIBS[] = {
    "shaderapidx9.dll",
    "engine.dll",
    "tier0.dll",
    "client.dll",
};

bool wait_for_game_libs()
{
    const char** libs = NULL;
    s32 num_libs = 0;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            libs = COMMON_LIBS;
            num_libs = SVR_ARRAY_SIZE(COMMON_LIBS);
            break;
        }

        default: assert(false);
    }

    if (!wait_for_libs_to_load(libs, num_libs))
    {
        return false;
    }

    return true;
}

// If the game has a restriction that prevents cvars from changing when in game or demo playback.
// This is the "switch to multiplayer or spectators" message.
// Remove cvar write restriction.
void patch_cvar_restrict()
{
    u8* addr = NULL;

    // This replaces the flags to compare to, so the comparison will always be false (removing cvar restriction).
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            addr = (u8*)pattern_scan("engine.dll", "68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 58 83 3D", __FUNCTION__);
            addr += 1;
            break;
        }

        case STEAM_GAME_BMS:
        {
            addr = (u8*)pattern_scan("engine.dll", "68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 52 83 3D", __FUNCTION__);
            addr += 1;
            break;
        }

        case STEAM_GAME_CSGO:
        {
            addr = (u8*)pattern_scan("engine.dll", "68 ?? ?? ?? ?? 8B 40 08 FF D0 84 C0 74 5D A1 ?? ?? ?? ?? 83 B8", __FUNCTION__);
            addr += 1;
            break;
        }

        default: assert(false);
    }

    if (addr)
    {
        u8 cvar_restrict_patch_bytes[] = { 0x00, 0x00, 0x00, 0x00 };
        apply_patch(addr, cvar_restrict_patch_bytes, SVR_ARRAY_SIZE(cvar_restrict_patch_bytes));
    }
}

IDirect3DDevice9Ex* get_d3d9ex_device()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            u8* addr = (u8*)pattern_scan("shaderapidx9.dll", "A1 ?? ?? ?? ?? 6A 00 56 6A 00 8B 08 6A 15 68 ?? ?? ?? ?? 6A 00 6A 01 6A 01 50 FF 51 5C 85 C0 79 06 C7 06", __FUNCTION__);
            addr += 1;
            return **(IDirect3DDevice9Ex***)addr;
        }

        default: assert(false);
    }

    return NULL;
}

void* get_engine_client_ptr()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_CSS:
        case STEAM_GAME_CSGO:
        case STEAM_GAME_TF2:
        {
            return create_interface("engine.dll", "VEngineClient014");
        }

        case STEAM_GAME_BMS:
        {
            return create_interface("engine.dll", "VEngineClient015");
        }

        default: assert(false);
    }

    return NULL;
}

void* get_local_or_spec_target_fn()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSGO:
        {
            return pattern_scan("client.dll", "55 8B EC 8B 4D 04 56 57 E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 85 F6 74 57 8B 06 8B CE", __FUNCTION__);
        }

        default: assert(false);
    }

    return NULL;
}

void* get_local_player_ptr()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            u8* addr = (u8*)pattern_scan("client.dll", "A3 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B C8 E8", __FUNCTION__);
            addr += 1;
            return addr;
        }

        case STEAM_GAME_CSGO:
        {
            u8* addr = (u8*)pattern_scan("client.dll", "8B 35 ?? ?? ?? ?? 85 F6 74 2E 8B 06 8B CE FF 50 28", __FUNCTION__);
            addr += 2;
            return addr;
        }

        default: assert(false);
    }

    return NULL;
}

void* get_engine_client_exec_cmd_fn(void* engine_client_ptr)
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            return get_virtual(engine_client_ptr, 102);
        }

        case STEAM_GAME_BMS:
        case STEAM_GAME_CSGO:
        {
            return get_virtual(engine_client_ptr, 108);
        }

        default: assert(false);
    }

    return NULL;
}

void* get_signon_state_ptr()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            u8* addr = (u8*)pattern_scan("engine.dll", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 87 ?? ?? ?? ?? 89 87 ?? ?? ?? ?? 8B 45 08", __FUNCTION__);
            addr += 2;
            return addr;
        }

        case STEAM_GAME_CSGO:
        {
            u8* addr = (u8*)pattern_scan("engine.dll", "A1 ?? ?? ?? ?? 33 D2 6A 00 6A 00 33 C9 C7 80", __FUNCTION__);
            addr += 1;

            void* client_state = **(void***)addr;
            addr = (u8*)client_state;
            addr += 264;

            return addr;
        }

        default: assert(false);
    }

    return NULL;
}

FnOverride get_eng_filter_time_override()
{
    FnOverride ov;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 51 80 3D ?? ?? ?? ?? ?? 56 8B F1 74", __FUNCTION__);
            ov.hook = eng_filter_time_override;
            break;
        }

        case STEAM_GAME_BMS:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 83 EC 10 80 3D ?? ?? ?? ?? ?? 56", __FUNCTION__);
            ov.hook = eng_filter_time_override;
            break;
        }

        case STEAM_GAME_CSGO:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 83 EC 0C 80 3D ?? ?? ?? ?? ?? 56", __FUNCTION__);
            ov.hook = eng_filter_time_override2;
            break;
        }

        default: assert(false);
    }

    return ov;
}

FnOverride get_start_movie_override()
{
    FnOverride ov;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 83 EC 08 83 3D ?? ?? ?? ?? ?? 0F 85", __FUNCTION__);
            ov.hook = start_movie_override;
            break;
        }

        case STEAM_GAME_CSGO:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 83 EC 08 53 56 57 8B 7D 08 8B 1F 83 FB 02 7D 5F", __FUNCTION__);
            ov.hook = start_movie_override;
            break;
        }

        default: assert(false);
    }

    return ov;
}

FnOverride get_end_movie_override()
{
    FnOverride ov;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            ov.target = pattern_scan("engine.dll", "80 3D ?? ?? ?? ?? ?? 75 0F 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 04 C3 E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 59 C3", __FUNCTION__);
            ov.hook = end_movie_override;
            break;
        }

        case STEAM_GAME_CSGO:
        {
            ov.target = pattern_scan("engine.dll", "80 3D ?? ?? ?? ?? ?? 75 0F 68", __FUNCTION__);
            ov.hook = end_movie_override;
            break;
        }

        default: assert(false);
    }

    return ov;
}

void* get_get_spec_target_fn()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            return pattern_scan("client.dll", "E8 ?? ?? ?? ?? 85 C0 74 16 8B 10 8B C8 FF 92 ?? ?? ?? ?? 85 C0 74 08 8D 48 08 8B 01 FF 60 24 33 C0 C3", __FUNCTION__);
        }

        case STEAM_GAME_CSGO:
        {
            return pattern_scan("client.dll", "55 8B EC 8B 4D 04 8B C1 83 C0 08 8B 0D ?? ?? ?? ?? 85 C9 74 15 8B 01 FF 90 ?? ?? ?? ?? 85 C0 74 09 8D 48 08 8B 01 5D FF 60 28", __FUNCTION__);
        }

        default: assert(false);
    }

    return NULL;
}

void* get_get_player_by_index_fn()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            return pattern_scan("client.dll", "55 8B EC 8B 0D ?? ?? ?? ?? 56 FF 75 08 E8 ?? ?? ?? ?? 8B F0 85 F6 74 15 8B 16 8B CE 8B 92 ?? ?? ?? ?? FF D2 84 C0 74 05 8B C6 5E 5D C3 33 C0 5E 5D C3", __FUNCTION__);
        }

        case STEAM_GAME_CSGO:
        {
            return pattern_scan("client.dll", "83 F9 01 7C ?? A1 ?? ?? ?? ?? 3B 48 ?? 7F ?? 56", __FUNCTION__);
        }

        default: assert(false);
    }

    return NULL;
}

FnOverride get_snd_paint_chans_override()
{
    FnOverride ov;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 81 EC ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53 33 DB 89 5D D0 89 5D D4", __FUNCTION__);
            ov.hook = snd_paint_chans_override;
            break;
        }

        case STEAM_GAME_HDTF:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 81 EC ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53 33 DB 89 5D C8 89 5D CC", __FUNCTION__);
            ov.hook = snd_paint_chans_override;
            break;
        }

        case STEAM_GAME_BMS:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 81 EC C4 01 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 ?? 8B 0D", __FUNCTION__);
            ov.hook = snd_paint_chans_override;
            break;
        }

        case STEAM_GAME_CSGO:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 81 EC ?? ?? ?? ?? A0 ?? ?? ?? ?? 53 56 88 45 ?? A1", __FUNCTION__);
            ov.hook = snd_paint_chans_override2;
            break;
        }

        default: assert(false);
    }

    return ov;
}

FnOverride get_snd_tx_stereo_override()
{
    FnOverride ov;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_BMS:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            ov.target = pattern_scan("engine.dll", "55 8B EC 51 53 56 57 E8 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D", __FUNCTION__);
            ov.hook = snd_tx_stereo_override;
            break;
        }

        default: assert(false);
    }

    return ov;
}

void* get_snd_paint_time_ptr()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_ZPS:
        case STEAM_GAME_SYNERGY:
        case STEAM_GAME_HL2:
        case STEAM_GAME_EMPIRES:
        case STEAM_GAME_HL2DM:
        case STEAM_GAME_HDTF:
        case STEAM_GAME_CSS:
        case STEAM_GAME_TF2:
        {
            u8* addr = (u8*)pattern_scan("engine.dll", "2B 05 ?? ?? ?? ?? 0F 48 C1 89 45 FC 85 C0", __FUNCTION__);
            addr += 2;
            return addr;
        }

        case STEAM_GAME_BMS:
        {
            u8* addr = (u8*)pattern_scan("engine.dll", "2B 35 ?? ?? ?? ?? 0F 48 F0", __FUNCTION__);
            addr += 2;
            return addr;
        }

        case STEAM_GAME_CSGO:
        {
            u8* addr = (u8*)pattern_scan("engine.dll", "66 0F 13 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 51 68", __FUNCTION__);
            addr += 4;
            return addr;
        }

        default: assert(false);
    }

    return NULL;
}

FnOverride get_snd_device_tx_override()
{
    FnOverride ov;

    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSGO:
        {
            ov.target = pattern_scan("engine.dll", "53 8B DC 83 EC 08 83 E4 F0 83 C4 04 55 8B 6B 04 89 6C 24 04 8B EC B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1", __FUNCTION__);
            ov.hook = snd_device_tx_override;
            break;
        }

        default: assert(false);
    }

    return ov;
}

void* get_snd_paint_buffer()
{
    switch (launcher_data.app_id)
    {
        case STEAM_GAME_CSGO:
        {
            u8* addr = (u8*)pattern_scan("engine.dll", "8B 35 ?? ?? ?? ?? 89 45 F8 A1 ?? ?? ?? ?? 57 8B 3D ?? ?? ?? ?? 89 45 FC", __FUNCTION__);
            addr += 2;
            return addr;
        }

        default: assert(false);
    }

    return NULL;
}

void create_game_hooks()
{
    // It's impossible to know whether or not these will actually point to the right thing. We cannot verify it so therefore we don't.
    // If any turns out to point to the wrong thing, we get a crash. Patterns must be updated in such case. The launcher and log will say what
    // build has been started, and we know what build we have been testing against.

    gm_d3d9ex_device = get_d3d9ex_device();
    gm_engine_client_ptr = get_engine_client_ptr();
    gm_engine_client_exec_cmd_fn = get_engine_client_exec_cmd_fn(gm_engine_client_ptr);
    gm_signon_state_ptr = get_signon_state_ptr();

    if (can_use_velo())
    {
        gm_local_player_ptr = get_local_player_ptr();

        if (launcher_data.app_id == STEAM_GAME_CSGO)
        {
            gm_local_or_spec_target_fn = get_local_or_spec_target_fn();
        }

        else
        {
            gm_get_spec_target_fn = get_get_spec_target_fn();
            gm_get_player_by_index_fn = get_get_player_by_index_fn();
        }
    }

    gm_snd_paint_time_ptr = get_snd_paint_time_ptr();

    hook_function(get_start_movie_override(), &start_movie_hook);
    hook_function(get_end_movie_override(), &end_movie_hook);
    hook_function(get_eng_filter_time_override(), &eng_filter_time_hook);

    hook_function(get_snd_paint_chans_override(), &snd_mix_chans_hook);

    // Game specific stuff.

    if (launcher_data.app_id == STEAM_GAME_CSGO)
    {
        hook_function(get_snd_device_tx_override(), &snd_device_tx_hook);
        gm_snd_paint_buffer = get_snd_paint_buffer();
    }

    else
    {
        hook_function(get_snd_tx_stereo_override(), &snd_tx_stereo_hook);
    }

    if (disable_window_update)
    {
        FnOverride present_ov;
        present_ov.target = get_virtual(gm_d3d9ex_device, 17); // Present.
        present_ov.hook = d3d9ex_present_override;
        hook_function(present_ov, &d3d9ex_present_hook);

        FnOverride present_ex_ov;
        present_ex_ov.target = get_virtual(gm_d3d9ex_device, 121); // PresentEx.
        present_ex_ov.hook = d3d9ex_present_ex_override;
        hook_function(present_ex_ov, &d3d9ex_present_ex_hook);
    }

    patch_cvar_restrict();

    // All other threads will be frozen during this call.
    MH_EnableHook(MH_ALL_HOOKS);
}

void read_command_line()
{
    char* start_args = GetCommandLineA();

    enable_autostop = true;

    // Doesn't belong in a profile so here it is.
    if (strstr(start_args, "-svrnoautostop"))
    {
        svr_log("Autostop is disabled\n");
        enable_autostop = false;
    }

    if (strstr(start_args, "-svrnowindupd"))
    {
        svr_log("Window update is disabled\n");
        disable_window_update = true;
    }
}

DWORD WINAPI standalone_init_async(void* param)
{
    // We don't want to show message boxes on failures because they work real bad in fullscreen.
    // Need to come up with some other mechanism.

    char log_file_path[MAX_PATH];
    log_file_path[0] = 0;
    StringCchCatA(log_file_path, MAX_PATH, launcher_data.svr_path);
    StringCchCatA(log_file_path, MAX_PATH, "\\data\\SVR_LOG.txt");

    // Append to the log file the launcher created.
    svr_init_log(log_file_path, true);

    // Need to notify that we have started because a lot of things can go wrong in standalone launch.
    svr_log("Hello from the game\n");

    if (!wait_for_game_libs())
    {
        svr_log("Not all libraries loaded in time\n");
        standalone_error("Mismatch between game version and supported SVR version. Ensure you are using the latest version of SVR and upload your SVR_LOG.txt.");
        return 1;
    }

    game_console_init();

    read_command_line();

    MH_Initialize();

    create_game_hooks();

    if (!svr_init(launcher_data.svr_path, gm_d3d9ex_device))
    {
        standalone_error("Could not initialize SVR. Ensure you are using the latest version of SVR and upload your SVR_LOG.txt.");
        return 1;
    }

    svr_init_prof();

    // It's useful to show that we have loaded when in standalone mode.
    // This message may not be the latest message but at least it's in there.

    game_console_msg("-------------------------------------------------------\n");
    game_console_msg("SVR initialized\n");
    game_console_msg("-------------------------------------------------------\n");

    return 0;
}

// Called when launching by the standalone launcher. This is before the process has started, and there are no game libraries loaded here.
extern "C" __declspec(dllexport) void svr_init_from_launcher(SvrGameInitData* init_data)
{
    launcher_data = *init_data;
    main_thread_id = GetCurrentThreadId();

    // Init needs to be done async because we need to wait for the libraries to load while the game loads as normal.
    CreateThread(NULL, 0, standalone_init_async, NULL, 0, NULL);
}
