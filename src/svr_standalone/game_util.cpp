#include "game_priv.h"
#include "game_common.h"

void* game_create_interface(const char* dll, const char* name)
{
    using CreateInterfaceFn = void* (__cdecl*)(const char* name, s32* code);
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

// Find bytes in the data sections of a dll.
u8* game_find_in_data(u8* base, void* bytes, s32 size, s32 step)
{
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + ((IMAGE_DOS_HEADER*)base)->e_lfanew);
    IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);

    for (s32 i = 0; i < nt->FileHeader.NumberOfSections; i++, section++)
    {
        if ((section->Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_DISCARDABLE)) || !(section->Characteristics & IMAGE_SCN_MEM_READ))
        {
            continue;
        }

        u8* start = base + section->VirtualAddress;
        u8* end = start + section->Misc.VirtualSize;

        for (u8* addr = start; addr + size <= end; addr += step)
        {
            if (memcmp(addr, bytes, size) == 0)
            {
                return addr;
            }
        }
    }

    return NULL;
}

void** game_find_vtable(const char* dll, const char* class_name)
{
    u8* base = (u8*)GetModuleHandleA(dll);

    if (base == NULL)
    {
        return NULL;
    }

    // Find the class name, then the RTTI that points to the name, then the vtable that points to the RTTI.

    char name[256];
    StringCchPrintfA(name, SVR_ARRAY_SIZE(name), ".?AV%s@@", class_name);

    u8* name_addr = game_find_in_data(base, name, (s32)strlen(name) + 1, 1);

    if (name_addr == NULL)
    {
        return NULL;
    }

    // Start of the RTTI, ends with where the class name is.
    u32 locator_start[4] = { 1, 0, 0, (u32)(name_addr - 16 - base) };

    u8* locator = game_find_in_data(base, locator_start, sizeof(locator_start), 4);

    if (locator == NULL)
    {
        return NULL;
    }

    u8* locator_ref = game_find_in_data(base, &locator, sizeof(locator), 8);

    if (locator_ref == NULL)
    {
        return NULL;
    }

    return (void**)(locator_ref + 8);
}

void* game_get_export(const char* dll, const char* name)
{
    HMODULE module = GetModuleHandleA(dll);
    assert(module);

    return GetProcAddress(module, name);
}

void game_apply_patch(void* target, void* bytes, s32 num_bytes, GameStoredPatch* orig)
{
    if (orig)
    {
        // Save original bytes so we can restore.

        assert(num_bytes <= SVR_ARRAY_SIZE(GameStoredPatch::orig_bytes));

        orig->num_bytes = num_bytes;
        memcpy(orig->orig_bytes, target, orig->num_bytes);
    }

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

void* game_get_module_base(const char* dll)
{
    MODULEINFO info;

    if (!GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(dll), &info, sizeof(MODULEINFO)))
    {
        // Module is not loaded. Not an error because we allow fallthrough scanning of multiple patterns.
        return NULL;
    }

    return info.lpBaseOfDll;
}
