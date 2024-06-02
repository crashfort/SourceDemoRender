#pragma once

const s32 RENDER_QUEUED_FRAMES = 8192; // Max number of uncompressed AVFrame* to queue up for encoding.
const s32 RENDER_QUEUED_PACKETS = 8192; // Max number of compressed AVPacket* to queue up for writing.
const s32 VID_QUEUED_TEXTURES = 16; // Max number of converted uncompressed frames to store in RAM before encode.
const s32 RENDER_QUEUED_AUDIO_BUFFERS = 8192; // Max number of audio buffers to queue up for conversion and encoding.
const s32 VID_MAX_PLANES = 3; // At most, YUV uses 3 planes.
const s32 AUDIO_MAX_CHANS = 8;

struct RenderVideoInfo;
struct RenderAudioInfo;

struct RenderFrameThreadInput
{
    AVCodecContext* ctx;
    AVFrame* frame;
    AVStream* stream;
    AVMediaType type;
};

struct RenderAudioThreadInput
{
    void* mem; // In the format incoming from svr_game. Capacity is always ENCODER_MAX_SAMPLES.
    s32 num_samples; // How many samples there actually are.
};

struct VidTextureDownloadInput
{
    ID3D11Texture2D* dl_texs[VID_MAX_PLANES]; // In system memory.
};

struct EncoderShader
{
    const char* name;
    void** dest;
    D3D11_SHADER_TYPE type;
};

struct EncoderState
{
    // -----------------------------------------------
    // Game and program state:

    EncoderSharedMem* shared_mem_ptr;

    HANDLE game_process;
    HANDLE shared_mem_h;
    HANDLE game_wake_event_h;
    HANDLE encoder_wake_event_h;
    HANDLE game_texture_h;
    void* shared_audio_buffer; // Where svr_game will put the audio samples.

    DWORD main_thread_id;

    EncoderSharedMovieParams movie_params; // Copied from the shared memory on movie start.

    bool init(HANDLE in_shared_mem_h);

    void start_event();
    void stop_event();
    void new_video_frame_event();
    void new_audio_samples_event();
    void event_loop();

    void free_static();
    void free_dynamic();

    // Use this on any error.
    // Prints to our log and also copies to shared memory where it is displayed in the game console and game log.
    // This can only be used by the main thread!
    void error(const char* format, ...);

    // -----------------------------------------------
    // Render state:

    AVFormatContext* render_output_context;
    const AVOutputFormat* render_container;

    SVR_THREAD_PADDING();

    SvrAtom32 render_started;

    // The threads start when rendering starts, and stop when rendering stops.
    // This makes it really easy to synchronize when stopping.

    // Frame thread:

    SVR_THREAD_PADDING();

    HANDLE render_frame_thread_h; // Thread used to process uncompressed video frames and audio samples.

    // Event set by the main and audio threads to notify that there are new frames to encode.
    HANDLE render_frame_wake_event_h;

    // Uncompressed frames and samples ready to be encoded.
    // Written to by the main and audio threads, read by the frame thread.
    // Order matters.
    SvrLockedQueue<RenderFrameThreadInput> render_frame_queue;

    // Video frames that have been encoded.
    // Written to by the frame thread, read by the main thread.
    // Order doesn't matter.
    SvrLockedArray<AVFrame*> render_recycled_video_frames;

    // Audio frames that have been encoded.
    // Written to by the frame thread, read by the main thread.
    // Order doesn't matter.
    SvrLockedArray<AVFrame*> render_recycled_audio_frames;

    SvrAtom32 render_frame_thread_status; // Will be set to 0 by frame thread if it failed. Message will be in render_frame_thread_message.
    char render_frame_thread_message[256]; // Error message for the frame thread.

    // Packet thread:

    SVR_THREAD_PADDING();

    HANDLE render_packet_thread_h; // Thread used to process encoded packets for writing to the container.

    // Event set by the frame thread to notify that there are encoded packets to write.
    // When rendering stops, this will be set by the main thread instead.
    HANDLE render_packet_wake_event_h;

    // Compressed packets ready to be written.
    // Written to by the frame thread, read by the packet thread.
    // When rendering stops, this will be written to by the main thread instead.
    // Order matters.
    SvrLockedQueue<AVPacket*> render_packet_queue;

    SvrAtom32 render_packet_thread_status; // Will be set to 0 by packet thread if it failed. Message will be in render_packet_thread_message.
    char render_packet_thread_message[256]; // Error message for the packet thread.

    // Audio thread:

    SVR_THREAD_PADDING();

    HANDLE render_audio_thread_h; // Thread used to process incoming audio buffers for conversion and encoding.

    // Event set by the main thread to notify that there are new audio buffers to process.
    HANDLE render_audio_wake_event_h;

    // Uncompressed audio samples ready to be converted and encoded.
    // Written to by the main thread, read by the audio thread.
    // Order matters.
    SvrLockedQueue<RenderAudioThreadInput> render_audio_queue;

    // Raw audio buffers.
    // Written to by the audio thread, read by the main thread.
    // Order does not matter.
    SvrLockedArray<RenderAudioThreadInput> render_recycled_audio_buffers;

    SvrAtom32 render_audio_thread_status; // Will be set to 0 by audio thread if it failed. Message will be in render_audio_thread_message.
    char render_audio_thread_message[256]; // Error message for the audio thread.

    SVR_THREAD_PADDING();

    const RenderVideoInfo* render_video_info;
    AVStream* render_video_stream;
    AVCodecContext* render_video_ctx;
    s64 render_video_pts; // Presentation timestamp.

    const RenderAudioInfo* render_audio_info;
    AVStream* render_audio_stream;
    AVCodecContext* render_audio_ctx;

    SVR_THREAD_PADDING();

    s64 render_audio_pts; // Presentation timestamp. Set by the audio thread.

    SVR_THREAD_PADDING();

    bool render_init();
    bool render_start();
    bool render_start_threads();
    void render_free_static();
    void render_free_dynamic();
    void render_frame_proc();
    void render_packet_proc();
    void render_audio_proc();
    bool render_setup_video_info();
    bool render_setup_audio_info();
    bool render_init_output_context();
    bool render_init_video();
    bool render_init_audio();
    bool render_check_thread_errors();
    bool render_receive_video();
    bool render_receive_audio();
    void render_give_audio_thread_input(RenderAudioThreadInput* input);
    void render_flush_audio_fifo();
    void render_submit_audio_fifo();
    void render_encode_frame_from_audio_fifo(s32 num_samples);
    void render_encode_video_frame(AVFrame* frame);
    void render_encode_audio_frame(AVFrame* frame);
    void render_encode_frame(AVCodecContext* ctx, AVStream* stream, AVFrame* frame, AVMediaType type);
    AVFrame* render_get_new_video_frame();
    AVFrame* render_get_new_audio_frame();
    RenderAudioThreadInput render_get_new_audio_buffer(s32 num_samples);
    s32 render_get_audio_buffer_size(s32 num_samples);
    void render_free_recycled_stuff();
    void render_free_lingering_thread_inputs();
    void render_submit_texture();

    void render_setup_dnxhr();
    void render_setup_libx264();

    // -----------------------------------------------
    // Video state:

    ID3D11Device1* vid_d3d11_device;
    ID3D11DeviceContext* vid_d3d11_context;
    void* vid_shader_mem;
    s32 vid_shader_size;

    ID3D11Texture2D* vid_game_tex; // Texture that svr_game updates.
    ID3D11ShaderResourceView* vid_game_tex_srv;
    IDXGIKeyedMutex* vid_game_tex_lock;

    ID3D11ComputeShader* vid_conversion_cs;
    s32 vid_num_planes;
    s32 vid_plane_heights[VID_MAX_PLANES];

    ID3D11ComputeShader* vid_nv12_cs;
    ID3D11ComputeShader* vid_yuv422_cs;
    ID3D11ComputeShader* vid_yuv444_cs;

    // Destination textures that are in the correct pixel format.
    // These textures have the actual data that can be encoded.
    // In order to not stall the pipeline by immediately trying to download the result,
    // the results are stored in vid_texture_download_queue until the commands have been processed.
    // Only after some time has passed it is safe to try and download without causing a stall.
    ID3D11Texture2D* vid_converted_texs[VID_MAX_PLANES];
    ID3D11UnorderedAccessView* vid_converted_uavs[VID_MAX_PLANES];

    VidTextureDownloadInput* vid_texture_download_queue;

    // These indexes get wrapped.
    s64 render_download_write_idx;
    s64 render_download_read_idx;

    bool vid_init();
    bool vid_create_device();
    bool vid_create_shaders();
    void vid_free_static();
    void vid_free_dynamic();
    bool vid_load_shader(const char* name);
    bool vid_create_shader(const char* name, void** shader, D3D11_SHADER_TYPE type);
    bool vid_create_shaders_list(EncoderShader* shaders, s32 num);
    bool vid_start();
    bool vid_open_game_texture();
    void vid_create_conversion_texs();
    void vid_push_texture_for_conversion();
    void vid_download_texture_into_frame(AVFrame* dest_frame);
    bool vid_can_map_now();
    bool vid_drain_textures();
    s32 vid_get_num_cs_threads(s32 unit);

    // -----------------------------------------------
    // Audio state:

    SwrContext* audio_swr;

    s32 audio_input_hz;
    s32 audio_output_hz;
    s32 audio_num_channels; // Input and output use the same.

    u8* audio_output_buffers[AUDIO_MAX_CHANS];
    s32 audio_output_size; // Size of allocated audio buffers.

    AVAudioFifo* audio_fifo;

    bool audio_init();
    void audio_free_static();
    void audio_free_dynamic();
    bool audio_start();
    bool audio_create_resampler();
    bool audio_create_fifo();
    void audio_convert_to_codec_samples(RenderAudioThreadInput* buffer);
    void audio_copy_samples_to_frame(AVFrame* dest_frame, s32 num_samples);
    s32 audio_num_queued_samples();
    bool audio_need_conversion();
};

struct RenderVideoInfo
{
    const char* profile_name; // Name as written in the movie profile.
    const char* codec_name; // Name in ffmpeg.
    AVPixelFormat pixel_format; // An encoder may support several pixel formats, so we select the one we like the most.

    // Set state according to the movie profile.
    // This is called before the codec is opened.
    void(EncoderState::*setup)();
};

struct RenderAudioInfo
{
    const char* profile_name; // Name as written in the movie profile.
    const char* codec_name; // Name in ffmpeg.
    AVSampleFormat sample_format; // An encoder may support several sample formats, so we select the one we like the most.

    // An encoder may support several sample rates, so we select the one we like the most.
    // Set to 0 to use the same as the input.
    s32 hz;

    // Set state according to the movie profile.
    // This is called before the codec is opened.
    void(EncoderState::*setup)();
};
