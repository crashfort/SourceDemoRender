#include "encoder_priv.h"

EncoderState encoder_state;

void av_log_callback(void* avcl, int level, const char* fmt, va_list vl)
{
    // Change this comparison if you need to see more detailed output.
    if (level > AV_LOG_WARNING)
    {
        return;
    }

    char buf[4096];
    SVR_VSNPRINTF(buf, fmt, vl);

    const char* format = NULL;

    // Some messages from FFmpeg will not end with a newline. We require that every message ends with a newline.
    if (!svr_ends_with(buf, "\n"))
    {
        format = "ffmpeg: %s\n";
    }

    else
    {
        format = "ffmpeg: %s";
    }

    svr_log(format, buf);

#ifdef SVR_DEBUG
    if (IsDebuggerPresent())
    {
        OutputDebugStringA(svr_va(format, buf));
    }
#endif
}

int main(int argc, char** argv)
{
#ifdef SVR_DEBUG
    _set_error_mode(_OUT_TO_MSGBOX); // Must be called so we can actually use assert because Microsoft messed it up in console builds.
#endif

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

    svr_log("SVR " SVR_ARCH_STRING " version %d (%02d/%02d/%04d %02d:%02d:%02d)\n", SVR_VERSION, lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond);
    svr_log("For more information see https://github.com/crashfort/SourceDemoRender\n");

    // We inherit handles when creating this process, so we can just read the handle address directly.
    // The encoder is 64-bit and the game is 32-bit, but all handles only have 32 bits significant, so this is safe.
    HANDLE shared_mem_h = (HANDLE)(u32)strtoul(argv[1], NULL, 10);

    encoder_state.init(shared_mem_h);
    encoder_state.event_loop();
    encoder_state.free_static();

    return 0;
}
