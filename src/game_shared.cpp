#include "game_shared.h"
#include <Windows.h>
#include <Psapi.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <MinHook.h>
#include <Psapi.h>
#include <charconv>
#include <assert.h>

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
    GetModuleFileNameExA(proc, (HMODULE)mod, temp, sizeof(temp));

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
            if (strcmp(list[j], name) == 0)
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

struct ScanPattern
{
    // Negative value means unknown byte.
    s16 bytes[256];
    s16 used = 0;
};

void pattern_bytes_from_string(const char* input, ScanPattern* out)
{
    bool expect_space = false;
    const char* it = input;

    while (*it)
    {
        if (*it == ' ')
        {
            ++it;
            expect_space = false;
        }

        assert(!expect_space);

        if ((*it >= '0' && *it <= '9') || (*it >= 'A' && *it <= 'F'))
        {
            auto res = std::from_chars(it, it + 2, out->bytes[out->used], 16);

            assert(res.ec == std::errc());

            out->used++;

            it += 2;

            expect_space = true;
        }

        else if (*it == '?')
        {
            out->bytes[out->used] = -1;
            out->used++;

            it += 2;

            expect_space = true;
        }
    }

    assert(out->used > 0);
}

bool compare_data(const u8* data, const ScanPattern& pattern)
{
    s32 index = 0;

    const s16* bytes = pattern.bytes;

    for (s32 i = 0; i < pattern.used; i++)
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

    return index == pattern.used;
}

void* find_pattern(void* start, s32 search_length, const ScanPattern& pattern)
{
    s16 length = pattern.used;

    for (s32 i = 0; i <= search_length - length; ++i)
    {
        u8* addr = static_cast<u8*>(start) + i;

        if (compare_data(addr, pattern))
        {
            return addr;
        }
    }

    return NULL;
}

bool game_wait_for_libs(const char** libs, s32 num)
{
    // Alternate method instead of hooking the LoadLibrary family of functions.
    // We don't need particular accuracy so this is good enough and much simpler.

    for (s32 i = 0; i < 5; i++)
    {
        if (check_loaded_proc_modules(libs, num) == num)
        {
            return true;
        }

        Sleep(2000);
    }

    return false;
}

void game_hook_function(void* target, void* hook, GameHook* result_hook)
{
    // Either this works or the process sinks, so no point handling errors.

    void* orig;

    MH_CreateHook(target, hook, &orig);

    result_hook->target = target;
    result_hook->hook = hook;
    result_hook->original = orig;
}

void* game_pattern_scan(const char* pattern, const char* module)
{
    MODULEINFO info;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(module), &info, sizeof(info));

    ScanPattern pattern_bytes;
    pattern_bytes_from_string(pattern, &pattern_bytes);

    return find_pattern(info.lpBaseOfDll, info.SizeOfImage, pattern_bytes);
}

void game_apply_patch(void* target, u8* bytes, s32 num_bytes)
{
    // Make writable.

    DWORD old_protect;
    VirtualProtect(target, num_bytes, PAGE_EXECUTE_READWRITE, &old_protect);

    u8* head = (u8*)target;

    for (s32 i = 0; i < num_bytes; i++)
    {
        head[i] = bytes[i];
    }

    VirtualProtect(target, num_bytes, old_protect, NULL);
}

void* game_create_interface(const char* name, const char* module)
{
    using ValveCreateInterfaceFun = void*(__cdecl*)(const char* name, int* code);

    HMODULE hmodule = GetModuleHandleA(module);
    ValveCreateInterfaceFun fun = (ValveCreateInterfaceFun)GetProcAddress(hmodule, "CreateInterface");

    int code;
    return fun(name, &code);
}

void* game_get_virtual(void* ptr, s32 index)
{
    void** vtable = *((void***)ptr);
    return vtable[index];
}

void* game_get_export(const char* name, const char* module)
{
    HMODULE hmodule = GetModuleHandleA(module);
    return GetProcAddress(hmodule, name);
}

void game_console_msg_v(const char* format, va_list va);

void game_log(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    game_log_v(format, va);
    va_end(va);
}

void game_log_v(const char* format, va_list va)
{
    svr_log_v(format, va);
    game_console_msg_v(format, va);
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    HWND* out_hwnd = (HWND*)lParam;
    *out_hwnd = hwnd;
    return FALSE;
}

void game_error(const char* format, ...)
{
    HWND game_hwnd;
    EnumThreadWindows(GetCurrentThreadId(), EnumWindowsProc, (LPARAM)&game_hwnd);

    char message[1024];

    va_list va;
    va_start(va, format);
    StringCchVPrintfA(message, 1024, format, va);
    va_end(va);

    svr_log("!!! GAME ERROR: %s\n", message);

    MessageBoxA(game_hwnd, message, "SVR", MB_ICONERROR | MB_OK);

    ExitProcess(1);
}
