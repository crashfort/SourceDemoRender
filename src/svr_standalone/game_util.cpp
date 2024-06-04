#include "game_priv.h"

void* game_create_interface(const char* dll, const char* name)
{
    using CreateInterfaceFn = void*(__cdecl*)(const char* name, s32* code);
    CreateInterfaceFn fn = (CreateInterfaceFn)game_get_export(dll, "CreateInterface");

    s32 code;
    return fn(name, &code);
}

void* game_get_virtual(void* ptr, s32 idx)
{
    if (ptr == NULL)
    {
        return NULL;
    }

    void** vtable = *((void***)ptr);
    return vtable[idx];
}

void* game_get_export(const char* dll, const char* name)
{
    HMODULE module = GetModuleHandleA(dll);
    assert(module);

    return GetProcAddress(module, name);
}

void game_apply_patch(void* target, void* bytes, s32 num_bytes)
{
    DWORD old_protect;
    VirtualProtect(target, num_bytes, PAGE_EXECUTE_READWRITE, &old_protect); // Make page writable.
    memcpy(target, bytes, num_bytes);
    VirtualProtect(target, num_bytes, old_protect, NULL); // Restore page.
}

bool game_is_valid(GameFnOverride ov)
{
    return ov.target;
}

bool game_is_valid(GameFnProxy px)
{
    return px.target;
}

void* game_follow_displacement(void* from, s32 length)
{
    u8* addr = (u8*)from;

    s32 offset = *(s32*)addr;
    addr += offset;
    addr += length;

    return addr;
}
