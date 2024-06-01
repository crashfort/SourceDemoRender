#pragma once

// -----------------------------------------------

// Features that a game has.
// If you add new caps, also add them to game_init_check_modules in game_init.cpp.
using GameCaps = u32;

enum /* GameCaps */
{
    GAME_CAP_HAS_CORE = SVR_BIT(0),
    GAME_CAP_HAS_VELO = SVR_BIT(1),
    GAME_CAP_HAS_AUDIO = SVR_BIT(2),
    GAME_CAP_HAS_VIDEO = SVR_BIT(3),
    GAME_CAP_HAS_AUTOSTOP = SVR_BIT(4),
    GAME_CAP_D3D9EX_VIDEO = SVR_BIT(5),
    GAME_CAP_AUDIO_DEVICE_1 = SVR_BIT(6),
    GAME_CAP_AUDIO_DEVICE_1_5 = SVR_BIT(7),
    GAME_CAP_AUDIO_DEVICE_2 = SVR_BIT(8),
    GAME_CAP_64_BIT_AUDIO_TIME = SVR_BIT(9),
    GAME_CAP_VELO_1 = SVR_BIT(10),
    GAME_CAP_VELO_2 = SVR_BIT(11),
};

// -----------------------------------------------
// game_video.cpp:

// Interface abstraction for the game video backend.
struct GameVideoDesc
{
    const char* name;

    void(*init)();
    void(*free)();
    IUnknown*(*get_game_device)();
    IUnknown*(*get_game_texture)();
};

void game_video_init();
void game_video_free();

// -----------------------------------------------
// game_audio.cpp:

// Interface abstraction for the game audio backend.
struct GameAudioDesc
{
    const char* name;

    void(*init)();
    void(*free)();
    void(*mix_audio_for_one_frame)();
};

void game_audio_init();
void game_audio_free();
void game_audio_frame();

// -----------------------------------------------

struct GameFnProxy;

// Generic function that takes some input and gives some output, and stores some static state from creation.
// Can be used to redirect game functions into a different generic signature through abstraction.
// Or can be used for reading from structures with unknown offsets set during creation.
using GameProxyFn = void(*)(GameFnProxy* proxy, void* params, void* res);

struct GameFnProxy
{
    void* target; // Proxy specific data.
    GameProxyFn proxy;
};

// -----------------------------------------------
// game_hook.cpp:

struct GameFnOverride
{
    void* target; // Address of function to override.
    void* override; // Detoured function.
};

struct GameFnHook
{
    void* target; // Address of function to override.
    void* override; // Detoured function.
    void* original; // Address of trampoline to jump to the original function.
};

void game_hook_init();
void game_hook_create(GameFnOverride* ov, GameFnHook* dest);
void game_hook_remove(GameFnHook* dest);
void game_hook_enable(GameFnHook* h, bool v);
void game_hook_enable_all();

// -----------------------------------------------
// game_search.cpp:

// Result structure after a search with game_search_fill_desc.
struct GameSearchDesc
{
    // Caps of this game that determines what functionality is available and what implementations to use.
    GameCaps caps;

    // Core required:
    GameFnOverride start_movie_override;
    GameFnOverride end_movie_override;
    GameFnOverride filter_time_override;
    GameFnProxy cvar_patch_restrict_proxy;
    GameFnProxy engine_client_command_proxy;
    GameFnProxy cmd_args_proxy;

    // Video D3D9EX required:
    GameFnProxy d3d9ex_device_proxy;

    // Velo required:
    GameFnProxy player_by_index_proxy;
    GameFnProxy entity_velocity_proxy;
    GameFnProxy spec_target_proxy;
    GameFnProxy local_player_proxy;
    GameFnProxy spec_target_or_local_player_proxy;

    // Audio required:
    GameFnProxy snd_paint_time_proxy;
    GameFnOverride snd_paint_chans_override;
    s32 snd_sample_rate;
    s32 snd_num_channels;
    s32 snd_bit_depth;

    // Audio device 1 required:
    GameFnOverride snd_tx_stereo_override;

    // Audio device 2 required:
    GameFnOverride snd_device_tx_samples_override;
    GameFnProxy snd_paint_buffer_proxy;

    // Autostop required:
    GameFnProxy signon_state_proxy;
    s32 signon_state_none;
    s32 signon_state_full;
};

void game_search_wait_for_libs();
void game_search_fill_desc(GameSearchDesc* desc); // Try and fill everything in the game description.

// -----------------------------------------------

using GameRecState = s32;

enum /* GameRecState */
{
    GAME_REC_STOPPED,
    GAME_REC_WAITING,
    GAME_REC_POSSIBLE,
};

struct GameState
{
    DWORD main_thread_id;

    const char* svr_path; // Absolute path to SVR. Does not end with a slash.

    GameSearchDesc search_desc;
    GameVideoDesc* video_desc;
    GameAudioDesc* audio_desc;

    GameFnHook start_movie_hook;
    GameFnHook end_movie_hook;
    GameFnHook filter_time_hook;

    GameFnHook snd_paint_chans_hook;
    GameFnHook snd_tx_stereo_hook;
    GameFnHook snd_device_tx_samples_hook;

    HWND wind_hwnd; // Main game window.
    char* wind_def_title; // Default title on start. Used to revert after recording.
    s64 wind_next_update_time; // Throttled update frequency because updating the title is slow.
    ITaskbarList3* wind_taskbar_list; // The taskbar progress bar.

    s64 rec_num_frames; // Number of processed frames.
    s64 rec_start_time; // Time of start for timing purposes.
    s32 rec_game_rate; // Frames per second the game is processing game at (includes motion blur).
    s32 rec_timeout; // After how many seconds to automatically end the movie.
    GameRecState rec_state; // Recording state tracking for autostop.
    bool rec_enable_autostop; // From start args: automatically stop on disconnect.
    bool rec_disable_window_update; // From start args: skip swap presentation.

    bool snd_is_painting; // Our signal to do specific paths during recording.
    bool snd_listener_underwater; // State variable from the engine.
    float snd_lost_mix_time; // Time that was lost between the fps to sample rate conversion. This is added back next frame.
    s32 snd_num_samples; // Used by audio variant 2.
    s32 snd_skipped_samples; // The number of samples to submit must align to 4 sample boundaries, that means there may be samples over that we have to process in the next frame.
};

extern GameState game_state;

// -----------------------------------------------
// game_libs.cpp:

// Returns true when all libraries or loaded, or false for timeout.
// Intended to be called from another thread than the thread that loads the libraries.
// Timeout is in seconds for how long to wait at most.
bool game_wait_for_libs_to_load(const char** libs, s32 num, s32 timeout);

// -----------------------------------------------
// game_scan.cpp:

// Scan a module for a byte sequence. Use "??" for unknown bytes.
// A start address can be specified to chain several pattern scans together.
void* game_scan_pattern(const char* dll, const char* pattern, void* from);

// -----------------------------------------------
// game_util.cpp:

// Create an engine interface in the given module.
void* game_create_interface(const char* dll, const char* name);

// Get a function from the virtual table by index.
void* game_get_virtual(void* ptr, s32 idx);

// Get an exported function in a library.
void* game_get_export(const char* dll, const char* name);

// Replace the bytes at the given address.
void game_apply_patch(void* target, void* bytes, s32 num_bytes);

// Used by game_search.cpp for the module validation arrays.
bool game_is_valid(GameFnOverride ov);
bool game_is_valid(GameFnProxy px);

// -----------------------------------------------
// game_overrides.cpp:

struct GameSndSample0
{
    s32 left;
    s32 right;
};

// Overriden functions built in the game:
void __cdecl game_snd_tx_stereo_override_0(void* unk, GameSndSample0* paint_buf, s32 paint_time, s32 end_time);
void __fastcall game_snd_device_tx_samples_override_0(void* p, void* edx, u32 unused);
void __cdecl game_snd_paint_chans_override_0(s32 end_time, bool is_underwater);
void __cdecl game_snd_paint_chans_override_1(s64 end_time, bool is_underwater);
void __cdecl game_start_movie_override_0(void* args);
void __cdecl game_end_movie_override_0(void* args);
bool __fastcall game_eng_filter_time_override_0(void* p, void* edx, float dt);

#ifndef _WIN64
bool __fastcall game_eng_filter_time_override_1(void* p, void* edx);
#endif

void game_overrides_init();

// -----------------------------------------------
// game_proxies.cpp:

// Proxies for redirecting stuff to or from the game:
void game_spec_target_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_engine_client_command_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_engine_client_command_proxy_1(GameFnProxy* proxy, void* params, void* res);
void game_player_by_index_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_velocity_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_cmd_args_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_signon_state_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_signon_state_proxy_1(GameFnProxy* proxy, void* params, void* res);
void game_paint_time_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_paint_time_proxy_1(GameFnProxy* proxy, void* params, void* res);
void game_paint_time_proxy_2(GameFnProxy* proxy, void* params, void* res);
void game_local_player_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_local_player_proxy_1(GameFnProxy* proxy, void* params, void* res);
void game_spec_target_or_local_player_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_paint_buffer_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_paint_buffer_proxy_1(GameFnProxy* proxy, void* params, void* res);
void game_cvar_restrict_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_d3d9ex_device_proxy_0(GameFnProxy* proxy, void* params, void* res);
void game_d3d9ex_device_proxy_1(GameFnProxy* proxy, void* params, void* res);

void game_engine_client_command(const char* cmd);
const char* game_get_cmd_args(void* ptr);
SvrVec3 game_get_entity_velocity(void* entity);
void* game_get_player_by_index(s32 idx);
void* game_get_local_player();
s32 game_get_spec_target();
void* game_get_spec_target_or_local_player();
s32 game_get_signon_state();
s32 game_get_snd_paint_time_0();
s64 game_get_snd_paint_time_1();
GameSndSample0* game_get_snd_paint_buffer_0();
void* game_get_d3d9ex_device();
void* game_get_cvar_patch_restrict();

// -----------------------------------------------
// game_init.cpp:

void game_init_error(const char* format, ...);

void game_init(SvrGameInitData* svr_init_data);
void game_init_log();
void game_init_async_proc();
void game_init_check_modules();

// -----------------------------------------------
// game_wind.cpp:

BOOL CALLBACK game_wind_enum_first_hwnd(HWND hwnd, LPARAM lparam);
void game_wind_early_init();
void game_wind_init();
void game_wind_free();
void game_wind_update_title(s64 now);
void game_wind_update_progress(s64 now);
void game_wind_update();
void game_wind_reset();

// -----------------------------------------------
// game_rec.cpp:

void game_rec_init();
void game_rec_update_timeout();
void game_rec_update_recording_state();
void game_rec_update_autostop();
void game_rec_show_start_movie_usage();
void game_rec_start_movie(void* cmd_args);
void game_rec_end_movie();
bool game_rec_run_frame();
void game_rec_do_record_frame();

// -----------------------------------------------
// game_cfg.cpp:

bool game_has_cfg(const char* name);
bool game_run_cfg(const char* name, bool required);
void game_run_cfgs_for_event(const char* name);

// -----------------------------------------------
// game_d3d9ex.cpp:

extern GameVideoDesc game_d3d9ex_desc;

// -----------------------------------------------
// game_audio_v1.cpp:

extern GameAudioDesc game_audio_v1_desc;

// -----------------------------------------------
// game_audio_v2.cpp:

extern GameAudioDesc game_audio_v2_desc;

// -----------------------------------------------
// game_velo.cpp:

void game_velo_frame();
void* game_velo_get_active_player();
void* game_velo_get_active_player_dumb();
void* game_velo_get_active_player_smart();

// -----------------------------------------------
