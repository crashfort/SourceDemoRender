#include "game_comp_base.hpp"

#include <svr/reverse.hpp>
#include <svr/game_config.hpp>
#include <svr/os.hpp>
#include <svr/str.hpp>
#include <svr/log_format.hpp>

#include <charconv>

// Scans a region of memory for a particular pattern.
static void* address_from_pattern(svr::game_config_comp* comp)
{
    using namespace svr;

    auto& value = comp->pattern_value;

    os_module_info info;

    if (!os_get_module_info(value.library, &info))
    {
        log("Could not get module information from '{}'\n", value.library);
        return nullptr;
    }

    auto address = reverse_find_pattern(info.base, info.size, value.pattern);

    if (address == nullptr)
    {
        return nullptr;
    }

    address = reverse_add_offset(address, value.offset);

    if (value.relative_jump)
    {
        address = reverse_follow_rel_jump(address);
    }

    return address;
}

static void* address_from_patch(svr::game_config_comp* comp)
{
    using namespace svr;

    auto& value = comp->patch_value;

    os_module_info info;

    if (!os_get_module_info(value.library, &info))
    {
        log("Could not get module information from '{}'\n", value.library);
        return nullptr;
    }

    auto address = reverse_find_pattern(info.base, info.size, value.pattern);

    if (address == nullptr)
    {
        return nullptr;
    }

    return reverse_add_offset(address, value.offset);
}

// Retrieves a pointer to a particular interface in a Valve module.
static void* address_from_create_interface(svr::game_config_comp* comp)
{
    using namespace svr;

    auto& value = comp->create_interface_value;

    using function_type = void*(__cdecl*)(const char* name, int* code);

    auto mod = os_get_module(value.library);

    if (mod == nullptr)
    {
        log("Could not get module '{}'\n", value.library);
        return nullptr;
    }

    auto func = static_cast<function_type>(os_get_module_function(mod, "CreateInterface"));

    if (func == nullptr)
    {
        log("Could not find export CreateInterface in '{}'\n", value.library);
        return nullptr;
    }

    int code = 0;
    return func(value.interface_name, &code);
}

static void* address_from_virtual(void* ptr, svr::game_config_comp* comp)
{
    auto& value = comp->virtual_value;
    return svr::reverse_get_virtual(ptr, value.vtable_index);
}

static void* address_from_export(svr::game_config_comp* comp)
{
    auto& value = comp->export_value;

    auto mod = svr::os_get_module(value.library);

    if (mod == nullptr)
    {
        svr::log("Could not get module '{}'\n", value.library);
        return nullptr;
    }

    return svr::os_get_module_function(mod, value.export_name);
}

static ptrdiff_t offset_from_config(svr::game_config_comp* comp)
{
    return comp->offset_value.value;
}

static bool apply_patch(void* address, svr::game_config_comp* comp)
{
    using namespace svr;

    auto& value = comp->patch_value;

    uint8_t bytes[64];
    memset(bytes, 0, sizeof(bytes));

    auto it = value.replace;

    size_t length = 0;

    while (*it)
    {
        it = str_trim_left(it);

        if (*it == 0)
        {
            break;
        }

        std::from_chars(it, it + 2, bytes[length], 16);
        length++;
        it += 2;
    }

    if (length == 0)
    {
        log("Tried to apply an empty patch with '{}'\n", comp->config_id);
        return false;
    }

    if (!os_make_mem_writable(address, length))
    {
        log("Could not make memory range writable with '{}'\n", comp->config_id);
        return false;
    }

    memcpy(address, bytes, length);
    return true;
}

IDirect3DDevice9Ex* d3d9ex_device_ptr;
void* materials_ptr;
void(__fastcall* materials_get_bbuf_size_addr_000)(void* p, void* edx, int& width, int& height);
void* engine_client_ptr;
void(__fastcall* client_exec_cmd_addr_000)(void* p, void* edx, const char* str);
void(__cdecl* console_msg_addr_000)(const char* format, ...);
void(__fastcall* view_render_addr_000)(void* p, void* edx, void* rect);
void(__cdecl* start_movie_addr_000)(const void* args);
void(__cdecl* end_movie_addr_000)(const void* args);
ptrdiff_t console_cmd_arg_offset;
void* local_player_ptr;
int(__cdecl* get_spec_target_000)();
void*(__cdecl* get_player_by_index_000)(int value);
ptrdiff_t player_abs_velocity_offset;

svr::reverse_hook_template<decltype(view_render_addr_000)> view_render_hook_000;
svr::reverse_hook_template<decltype(start_movie_addr_000)> start_movie_hook_000;
svr::reverse_hook_template<decltype(end_movie_addr_000)> end_movie_hook_000;

void materials_get_backbuffer_size(int& width, int& height)
{
    if (materials_get_bbuf_size_addr_000)
    {
        materials_get_bbuf_size_addr_000(materials_ptr, nullptr, width, height);
    }
}

void exec_client_command(const char* value)
{
    if (client_exec_cmd_addr_000)
    {
        client_exec_cmd_addr_000(engine_client_ptr, nullptr, value);
    }
}

void cvar_set_value(const char* name, int value)
{
    fmt::memory_buffer buf;
    svr::format_with_null(buf, "{} {}\n", name, value);

    exec_client_command(buf.data());
}

void console_message(const char* value)
{
    if (console_msg_addr_000)
    {
        console_msg_addr_000(value);
    }
}

int get_spec_target()
{
    if (get_spec_target_000)
    {
        return get_spec_target_000();
    }

    return -1;
}

void* get_player_by_index(int value)
{
    if (get_player_by_index_000)
    {
        return get_player_by_index_000(value);
    }

    return nullptr;
}

enum comp_resolve_status
{
    COMP_STATUS_RESOLVED,
    COMP_STATUS_UNRESOLVED,
    COMP_STATUS_ERROR,
};

struct code_comp
{
    const char* component;
    const char* signature;
    comp_resolve_status(*func)(svr::game_config_comp* comp);
};

static comp_resolve_status validate_ptr(void* value)
{
    if (value == nullptr)
    {
        return COMP_STATUS_ERROR;
    }

    return COMP_STATUS_RESOLVED;
}

static comp_resolve_status d3d9ex_device_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            auto addr = address_from_pattern(comp);

            if (addr == nullptr)
            {
                return COMP_STATUS_ERROR;
            }

            d3d9ex_device_ptr = **(IDirect3DDevice9Ex***)addr;
            return validate_ptr(d3d9ex_device_ptr);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status materials_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_CREATE_INTERFACE:
        {
            materials_ptr = address_from_create_interface(comp);
            return validate_ptr(materials_ptr);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status get_bbuf_size_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_VIRTUAL:
        {
            materials_get_bbuf_size_addr_000 = (decltype(materials_get_bbuf_size_addr_000))address_from_virtual(materials_ptr, comp);
            return validate_ptr(materials_get_bbuf_size_addr_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status engine_client_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_CREATE_INTERFACE:
        {
            engine_client_ptr = address_from_create_interface(comp);
            return validate_ptr(engine_client_ptr);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status client_exec_cmd_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_VIRTUAL:
        {
            client_exec_cmd_addr_000 = (decltype(client_exec_cmd_addr_000))address_from_virtual(engine_client_ptr, comp);
            return validate_ptr(client_exec_cmd_addr_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status console_message_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_EXPORT:
        {
            console_msg_addr_000 = (decltype(console_msg_addr_000))address_from_export(comp);
            return validate_ptr(console_msg_addr_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status view_render_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            view_render_addr_000 = (decltype(view_render_addr_000))address_from_pattern(comp);
            return validate_ptr(view_render_addr_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status start_movie_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            start_movie_addr_000 = (decltype(start_movie_addr_000))address_from_pattern(comp);
            return validate_ptr(start_movie_addr_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status end_movie_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            end_movie_addr_000 = (decltype(end_movie_addr_000))address_from_pattern(comp);
            return validate_ptr(end_movie_addr_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status console_cmd_arg_offset_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_OFFSET:
        {
            console_cmd_arg_offset = offset_from_config(comp);
            return COMP_STATUS_RESOLVED;
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status local_player_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            local_player_ptr = address_from_pattern(comp);
            return validate_ptr(local_player_ptr);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status get_spec_target_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            get_spec_target_000 = (decltype(get_spec_target_000))address_from_pattern(comp);
            return validate_ptr(get_spec_target_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status get_player_by_index_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATTERN:
        {
            get_player_by_index_000 = (decltype(get_player_by_index_000))address_from_pattern(comp);
            return validate_ptr(get_player_by_index_000);
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status player_abs_vel_offset_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_OFFSET:
        {
            player_abs_velocity_offset = offset_from_config(comp);
            return COMP_STATUS_RESOLVED;
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

static comp_resolve_status cvar_remove_restrict_func_000(svr::game_config_comp* comp)
{
    using namespace svr;

    switch (comp->code_type)
    {
        case GAME_COMP_PATCH:
        {
            auto addr = address_from_patch(comp);

            if (addr == nullptr)
            {
                return COMP_STATUS_ERROR;
            }

            if (!apply_patch(addr, comp))
            {
                return COMP_STATUS_ERROR;
            }

            return COMP_STATUS_RESOLVED;
        }
    }

    return COMP_STATUS_UNRESOLVED;
}

// Signatures in the game config are matched up with a function here.
static code_comp code_comps[] = {
    code_comp { "d3d9ex-device", "**(IDirect3DDevice9Ex***)", d3d9ex_device_func_000 },
    code_comp { "materials-ptr", "void*", materials_func_000 },
    code_comp { "materials-get-bbuf-size", "void(__fastcall*)(void* p, void* edx, int& width, int& height)", get_bbuf_size_func_000 },
    code_comp { "engine-client-ptr", "void*", engine_client_func_000 },
    code_comp { "engine-client-exec-cmd", "void(__fastcall*)(void* p, void* edx, const char* str)", client_exec_cmd_func_000 },
    code_comp { "console-print-message", "void(__cdecl*)(const char* format, ...)", console_message_func_000 },
    code_comp { "view-render", "void(__fastcall*)(void* p, void* edx, void* rect)", view_render_func_000 },
    code_comp { "start-movie-cmd", "void(__cdecl*)(const void* args)", start_movie_func_000 },
    code_comp { "end-movie-cmd", "void(__cdecl*)(const void* args)", end_movie_func_000 },
    code_comp { "console-cmd-args-offset", "ptrdiff_t", console_cmd_arg_offset_func_000 },
    code_comp { "local-player-ptr", "void*", local_player_000 },
    code_comp { "get-spec-target", "int(__cdecl*)()", get_spec_target_func_000 },
    code_comp { "get-player-by-index", "void*(__cdecl*)(int index)", get_player_by_index_func_000 },
    code_comp { "player-abs-velocity-offset", "ptrdiff_t", player_abs_vel_offset_func_000 },
    code_comp { "cvar-remove-restrict", "0x00400000", cvar_remove_restrict_func_000 },
};

static code_comp* find_code_comp(const char* name, const char* signature)
{
    for (auto& entry : code_comps)
    {
        if (strcmp(entry.component, name) == 0 && strcmp(entry.signature, signature) == 0)
        {
            return &entry;
        }
    }

    return nullptr;
}

static bool resolve_component(svr::game_config_comp* comp)
{
    using namespace svr;

    log("Resolving component '{}' with signature '{}' and type {}\n", comp->config_id, comp->signature, comp->code_type);

    auto target = find_code_comp(comp->code_id, comp->signature);

    if (target)
    {
        auto status = target->func(comp);

        if (status == COMP_STATUS_RESOLVED)
        {
            return true;
        }

        if (status == COMP_STATUS_ERROR)
        {
            log("Component could not be created\n");
            return false;
        }
    }

    return false;
}

bool find_resolve_component(svr::game_config_game* game, const char* name)
{
    using namespace svr;

    auto comp = game_config_game_find_comp(game, name);

    if (comp == nullptr)
    {
        log("Component '{}' not found\n", name);
        return false;
    }

    if (!resolve_component(comp))
    {
        log("Component '{}' could not be resolved\n", name);
        return false;
    }

    return true;
}
