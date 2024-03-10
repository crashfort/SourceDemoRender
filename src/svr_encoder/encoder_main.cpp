#include "svr_defs.h"
#include "encoder_priv.h"

EncoderState encoder_state;

void av_log_callback(void* avcl, int level, const char* fmt, va_list vl)
{
    // Change this comparison if you need to see more detailed output.
    if (level > AV_LOG_WARNING)
    {
        return;
    }

    // Some messages from FFmpeg will not end with a newline. We require that every message ends with a newline.
    if (!svr_ends_with(fmt, "\n"))
    {
        char buf[4096];
        vsnprintf(buf, SVR_ARRAY_SIZE(buf), fmt, vl);

        svr_log("ffmpeg: %s\n", buf);
        OutputDebugStringA(svr_va("ffmpeg: %s\n", buf));
    }

    else
    {
        svr_log_v(fmt, vl);
    }
}

int main(int argc, char** argv)
{
    svr_init_log("data\\ENCODER_LOG.txt", false);

    if (argc != 2)
    {
        svr_log("ERROR: Encoder has not been started properly. This program can not be started manually\n");
        return 1;
    }

    // For release this should be disabled.
    av_log_set_callback(av_log_callback);
    av_log_set_level(AV_LOG_WARNING);

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    svr_log("SVR version %d (%02d/%02d/%04d %02d:%02d:%02d)\n", SVR_VERSION, lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond);
    svr_log("For more information see https://github.com/crashfort/SourceDemoRender\n");

    // We inherit handles when creating this process, so we can just read the handle address directly.
    // The encoder is 64-bit and the game is 32-bit, but all handles only have 32 bits significant, so this is safe.
    HANDLE shared_mem_h = (HANDLE)(u32)strtoul(argv[1], NULL, 10);

    encoder_state.init(shared_mem_h);
    encoder_state.event_loop();
    encoder_state.free_static();

    return 0;
}
