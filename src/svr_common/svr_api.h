#pragma once
// Interface header for SVR.
// Don't use internal headers here such as svr_common, or internal type aliases, as this header should be easily distributed.
// This header is used by SVR standalone and by the Momentum integration.

// How this should be used:
// 1) At startup, compare the return value of svr_api_version with SVR_API_VERSION and continue if the versions match.
//    If the versions do not match then SVR cannot be used.
// 2) Call svr_init at an appropriate location to initialize SVR once.
// 3) Call svr_start when movie production should start.
// 4a) Call svr_frame for all frames where movie production is active.
// 4b) Optionally call svr_give_velocity before svr_frame.
// 5) Call svr_stop when movie production should stop.

// Programming errors are printed to the debugger output (prefixed with "SVR (<function name>):").
// User or system errors will print messages to SVR_LOG.txt (for standalone SVR) and/or to the game console (if available at the time of error).
// All functions in this API must be called from the game main thread only.

// Windows only.

#if SVR_GAME_DLL
#define SVR_API __declspec(dllexport)
#else
#define SVR_API __declspec(dllimport)
#endif

extern "C"
{

// To be increased when something in the interface changes. Internal DLL changes (svr_dll_version) does not have to up this.
// The API must not be used if the DLL API version does not match the client header API version.
const int SVR_API_VERSION = 1;

struct IUnknown;
struct IDirect3DSurface9;
struct IDirect3DDevice9Ex;
struct ID3D11ShaderResourceView;
struct ID3D11Device;

struct SvrStartMovieData
{
    // A view to a texture that contains the game content.
    // A reference will be added to this when starting, and released when stopping.
    // This should be a view to the swapchain backbuffer texture.
    // This can be either a ID3D11ShaderResourceView for D3D11 games or IDirect3DSurface9 for D3D9Ex games.
    // For D3D11 games the texture must be created with D3D11_BIND_RENDER_TARGET.
    IUnknown* game_tex_view;
};

struct SvrWaveSample
{
    short l;
    short r;
};

// For checking mismatch between built DLL and client header.
// Always call this and ensure that the versions match (compare to SVR_API_VERSION). The API should not be used if these mismatch, as it will most likely crash!
// You should not call svr_init (or any function at all) if there is a mismatch.
SVR_API int svr_api_version();

// SVR binary version. Marks new features or fixes.
SVR_API int svr_dll_version();

// To be called once at startup.
//
// The SVR path is the path to where SVR is located. This needs to be passed in because
// the game executable does not have to be located in the same directory structure as the game files (so a relative path wouldn't work, especially so because
// Source games tend to switch their working directory).
// For standalone SVR, this will be the path to where SVR is installed on the system.
// For integrated SVR, this will be the path to where SVR is located for the game (probably in a folder next to the executable).
//
// The game device should be either a ID3D11Device or a IDirect3DDevice9Ex.
// A reference will be added to the device.
//
// If game_device is a ID3D11Device, further resources will be created using it.
// The device must be created with a feature level greater or equal to D3D_FEATURE_LEVEL_12_0 and also with the flag D3D11_CREATE_DEVICE_BGRA_SUPPORT.
//
// If game_device is a IDirect3DDevice9Ex, then a new D3D11 device will be created, and additional resources will be created using both devices.
SVR_API bool svr_init(const char* svr_path, IUnknown* game_device);

// Returns whether or not movie processing is happening.
// This will return true after svr_start has succeeded and false after svr_stop.
SVR_API bool svr_movie_active();

// To be called when movie recording should start. Can be in response to a console command or UI element or some automatic event.
// Calling this function will create a media file but only when svr_frame is called will content be written.
//
// The movie name is the filename of the video that will be created, including the extension.
// Movies are saved in the SVR directory.
// The allowed extensions (media containers) for the H264 video stream is one of the following: mp4, mkv, mov.
// Starting the movie will fail if the container is not one of these. AVI is not supported as it's a very outdated
// container that does not support various H264 features.
//
// The movie profile is a name of a profile that contains encode details and more, located in the SVR directory.
//
// The following engine console variables should be adjusted after calling this function:
// *) fps_max should be set to 0 to not introduce any extra latency between frames.
// *) mat_queue_mode must be set to 0 because the queued rendering (value of 2) does not work.
// *) engine_no_focus_sleep should be set to 0 to allow the game not being in focus while processing.
// *) volume should be set to 0 for it to not be annoying.
//
// They can be reset back to their previous value after calling svr_stop.
//
// After calling this function, set host_framerate to the value returned by svr_get_game_rate.
// Load svr_movie_start.cfg after calling this.
SVR_API bool svr_start(const char* movie_name, const char* movie_profile, SvrStartMovieData* movie_data);

// This function should be called after svr_start to read how fast the game should be running.
// Set host_framerate to the value this returns.
SVR_API int svr_get_game_rate();

// To be called when movie recording should stop. Can be in response to a console command or UI element or some automatic event.
// Calling this function will stop movie production and calling svr_frame will not do anything.
// The console variables mentioned in svr_start can be reset back to their previous value after this. Also host_framerate must be set back to 0.
// Load svr_movie_end.cfg after calling this.
SVR_API void svr_stop();

// To be called when a new game frame has been rendered, but before it is presented (because the double buffered textures would be swapped).
// The texture (game_tex_view) that's passed in to svr_start will be encoded.
// This must only be called if svr_movie_active returns true.
SVR_API void svr_frame();

// Returns if velo is enabled in the active profile.
// You can use this to prevent extra work if it is not needed.
// Must only be called after svr_start.
SVR_API bool svr_is_velo_enabled();

// Returns if audio is enabled in the active profile.
// You can use this to prevent extra work if it is not needed.
// Must only be called after svr_start.
SVR_API bool svr_is_audio_enabled();

// For the velocity extension, call this to give the player xyz velocity so it can be drawn to the encoded video.
// Must be called before svr_frame.
SVR_API void svr_give_velocity(float* xyz);

// Give audio samples to write. This must be 16 bit samples at 44100 hz.
SVR_API void svr_give_audio(SvrWaveSample* samples, int num_samples);

}
