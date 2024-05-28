#pragma once

// Texture that comes directly from the game.
// This is read only and is managed by svr_api.
struct ProcGameTexture
{
    ID3D11Texture2D* tex;
    ID3D11ShaderResourceView* srv;
};

using ProcVeloAnchor = s32;

enum /* ProcVeloAnchor */
{
    VELO_ANCHOR_LEFT,
    VELO_ANCHOR_CENTER,
    VELO_ANCHOR_RIGHT,
};

using ProcVeloLength = s32;

enum /* ProcVeloLength */
{
    VELO_LENGTH_XY,
    VELO_LENGTH_XYZ,
    VELO_LENGTH_Z,
};

struct MovieProfile
{
    // Movie options:
    const char* video_encoder;
    const char* video_x264_preset;
    const char* video_dnxhr_profile;
    const char* audio_encoder;
    s32 video_fps;
    s32 video_x264_crf;
    s32 video_x264_intra;
    s32 audio_enabled;

    // Mosample options:
    s32 mosample_enabled;
    s32 mosample_mult;
    float mosample_exposure;

    // Velo options:
    s32 velo_enabled;
    char* velo_font; // Allocated.
    s32 velo_font_size;
    SvrVec4I velo_font_color;
    SvrVec4I velo_font_border_color;
    s32 velo_font_border_size;
    DWRITE_FONT_STYLE velo_font_style;
    DWRITE_FONT_WEIGHT velo_font_weight;
    SvrVec2I velo_align;
    ProcVeloAnchor velo_anchor;
    ProcVeloLength velo_length;
};

struct ProcState
{
    // -----------------------------------------------
    // Program state:

    char svr_resource_path[MAX_PATH]; // Does not end with a slash.

    ProcGameTexture svr_game_texture; // Texture of the game.
    SvrAudioParams svr_audio_params;

    bool init(const char* in_resource_path, ID3D11Device* in_d3d11_device);
    bool start(const char* dest_file, const char* profile, ProcGameTexture* game_texture, SvrAudioParams* audio_params);
    void new_video_frame();
    void new_audio_samples(SvrWaveSample* samples, s32 num_samples);
    bool is_velo_enabled();
    bool is_audio_enabled();
    void process_finished_shared_tex();
    void end();
    void free_static();
    void free_dynamic();
    s32 get_game_rate();

    // -----------------------------------------------
    // Video state:

    ID3D11Device* vid_d3d11_device;
    ID3D11DeviceContext* vid_d3d11_context;
    void* vid_shader_mem;
    s32 vid_shader_size;

    // Other 2D drawing.
    ID2D1Factory1* vid_d2d1_factory;
    ID2D1Device* vid_d2d1_device;
    ID2D1DeviceContext* vid_d2d1_context;
    IDWriteFactory* vid_dwrite_factory;
    ID2D1SolidColorBrush* vid_d2d1_solid_brush;

    bool vid_init(ID3D11Device* d3d11_device);
    bool vid_create_d2d1();
    bool vid_create_dwrite();
    void vid_free_static();
    void vid_free_dynamic();
    bool vid_load_shader(const char* name);
    bool vid_create_shader(const char* name, void** shader, D3D11_SHADER_TYPE type);
    void vid_update_constant_buffer(ID3D11Buffer* buffer, void* data, UINT size);
    void vid_clear_rtv(ID3D11RenderTargetView* rtv, float r, float g, float b, float a);
    s32 vid_get_num_cs_threads(s32 unit);
    bool vid_start();
    void vid_end();
    D2D1_COLOR_F vid_fill_d2d1_color(SvrVec4I color);
    D2D1_POINT_2F vid_fill_d2d1_pt(SvrVec2I p);

    // -----------------------------------------------
    // Velo state:

    IDWriteFontFace* velo_font_face;
    SvrVec2I velo_draw_pos;

    // Emulation of tabular font feature.
    float velo_tab_height;
    float velo_tab_advance_x;

    UINT16 velo_number_glyph_idxs[10]; // Glyph indexes for all numbers so we don't have to look that up every time.

    SvrVec3 velo_vector;

    bool velo_init();
    void velo_free_static();
    void velo_free_dynamic();
    bool velo_create_font_face();
    void velo_setup_tab_metrix();
    void velo_setup_glyph_idxs();
    bool velo_start();
    void velo_end();
    void velo_draw();
    void velo_give(SvrVec3 source);
    SvrVec2I velo_get_pos();
    float velo_get_length();

    // -----------------------------------------------
    // Motion blur state:

    // High precision texture used for the result of mosample (total 128 bits per pixel).
    ID3D11Texture2D* mosample_work_tex;
    ID3D11RenderTargetView* mosample_work_tex_rtv;
    ID3D11ShaderResourceView* mosample_work_tex_srv;
    ID3D11UnorderedAccessView* mosample_work_tex_uav;

    ID3D11ComputeShader* mosample_cs;
    ID3D11ComputeShader* mosample_downsample_cs;

    // Constains the mosample weight.
    ID3D11Buffer* mosample_cb;

    // To not upload data all the time.
    float mosample_weight_cache;

    float mosample_remainder;
    float mosample_remainder_step;

    bool mosample_init();
    bool mosample_create_buffer();
    bool mosample_create_shaders();
    bool mosample_create_textures();
    void mosample_free_static();
    void mosample_free_dynamic();
    bool mosample_start();
    void mosample_end();
    void mosample_process(float weight);
    void mosample_new_video_frame();
    void mosample_downsample_to_share_tex();

    // -----------------------------------------------
    // Encoder state:

    HANDLE encoder_proc;
    HANDLE encoder_shared_mem_h;
    HANDLE game_wake_event_h;
    HANDLE encoder_wake_event_h;
    EncoderSharedMem* encoder_shared_ptr;
    void* encoder_audio_buffer;

    // FIFO of audio samples we need to send to the encoder.
    // During motion blur capture, the number of samples sent from the game will be very low (like 12).
    // We should not wake up the encoder and wait for just that little, so queue up a bunch instead and send many.
    SvrDynQueue<SvrWaveSample> encoder_pending_samples;

    // Intermediate texture needed for texture sharing.
    // High precision textures are not allowed to be shared, so we need to downsample the result of the mosample to 32 bpp.
    // This texture is the final result from all prior processing, such as motion blur and velo text.
    ID3D11Texture2D* encoder_share_tex;
    ID3D11UnorderedAccessView* encoder_share_tex_uav;
    ID3D11RenderTargetView* encoder_share_tex_rtv;
    ID3D11ShaderResourceView* encoder_share_tex_srv;
    HANDLE encoder_share_tex_h;
    ID2D1Bitmap1* encoder_d2d1_share_tex; // Not a real texture, but a reference to encoder_share_tex.
    IDXGIKeyedMutex* encoder_share_tex_lock;

    bool encoder_init();
    void encoder_free_static();
    void encoder_free_dynamic();
    bool encoder_create_shared_mem();
    bool encoder_start_process();
    bool encoder_start();
    bool encoder_create_share_textures();
    bool encoder_set_shared_mem_params();
    void encoder_end();
    bool encoder_send_event(EncoderSharedEvent event);
    bool encoder_send_shared_tex();
    bool encoder_send_audio_samples(SvrWaveSample* samples, s32 num_samples);
    void encoder_flush_audio();
    bool encoder_submit_pending_samples();
    bool encoder_send_audio_from_pending(s32 num_samples);
    bool encoder_create_d2d1_bitmap();

    // -----------------------------------------------
    // Movie state:

    s32 movie_width;
    s32 movie_height;
    char movie_path[MAX_PATH];

    MovieProfile movie_profile;

    bool movie_init();
    void movie_free_static();
    void movie_free_dynamic();
    bool movie_start();
    void movie_end();
    void movie_setup_params();
    bool movie_load_profile(const char* name, bool required);
};
