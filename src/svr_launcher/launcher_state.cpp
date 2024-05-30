#include "launcher_priv.h"

void LauncherState::init()
{
    GetCurrentDirectoryA(MAX_PATH, working_dir);

    // Enable to show system information and stuff on start.
#if 1
    if (!IsWindows10OrGreater())
    {
        launcher_error("Windows 10 or later is needed to use SVR.");
    }

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    launcher_log("SVR " SVR_ARCH_STRING " version %d (%02d/%02d/%04d %02d:%02d:%02d)\n", SVR_VERSION, lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond);
    launcher_log("This is a standalone version of SVR. Interoperability with other applications may not work\n");
    launcher_log("For more information see https://github.com/crashfort/SourceDemoRender\n");

    sys_show_windows_version();
    sys_show_processor();
    sys_show_available_memory();
    sys_check_hw_caps();
#endif

    if (steam_find_path())
    {
        steam_find_libraries();
    }

    load_games();

    svr_log("Found %d games\n", game_list.size);
}

// Will put both to console and to file.
// Use printf for other messages that should not be shown in file.
// Use svr_log for messages that should not be shown on screen.
void LauncherState::launcher_log(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    svr_log_v(format, va);
    vprintf(format, va);
    va_end(va);
}

__declspec(noreturn) void LauncherState::launcher_error(const char* format, ...)
{
    char message[1024];

    va_list va;
    va_start(va, format);
    SVR_VSNPRINTF(message, format, va);
    va_end(va);

    svr_log("!!! LAUNCHER ERROR: %s\n", message);

    MessageBoxA(NULL, message, "SVR", MB_TASKMODAL | MB_ICONERROR | MB_OK);

    ExitProcess(1);
}

// Prompt user for an input number.
s32 LauncherState::get_choice_from_user(s32 min, s32 max)
{
    s32 selection = -1;

    while (selection == -1)
    {
        char buf[32];
        char* res = fgets(buf, SVR_ARRAY_SIZE(buf), stdin);

        if (res == NULL)
        {
            // Can get here from Ctrl+C.
            return -1;
        }

        selection = atoi(buf);

        if (selection <= min || selection > max)
        {
            selection = -1;
            continue;
        }
    }

    return selection - 1; // Numbers displayed are 1 based.
}
