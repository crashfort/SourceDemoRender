#include <svr/reverse.hpp>
#include <svr/os.hpp>
#include <svr/log_format.hpp>

#include <MinHook.h>

static svr::reverse_status convert_status(MH_STATUS value)
{
    using namespace svr;

    switch (value)
    {
        case MH_ERROR_ALREADY_CREATED: return REVERSE_STATUS_ALREADY_CREATED;
        case MH_ERROR_NOT_CREATED: return REVERSE_STATUS_NOT_CREATED;
        case MH_ERROR_NOT_EXECUTABLE: return REVERSE_STATUS_NOT_EXECUTABLE;
        case MH_ERROR_UNSUPPORTED_FUNCTION: return REVERSE_STATUS_FUNCTION_NOT_SUPPORTED;
    }

    return REVERSE_STATUS_OK;
}

namespace svr
{
    reverse_status reverse_hook_function(void* target, void* hook, reverse_hook* ptr)
    {
        log("reverse: Trying to hook function {} with {}\n", target, hook);

        void* orig;
        auto status = MH_CreateHook(target, hook, &orig);

        if (status == MH_OK)
        {
            ptr->target = target;
            ptr->hook = hook;
            ptr->original = orig;
        }

        else
        {
            log("reverse: Could not hook function '{}' with '{}' ({})\n", target, hook, MH_StatusToString(status));
        }

        return convert_status(status);
    }

    reverse_status reverse_hook_api_function(os_module* module, const char* name, void* hook, reverse_hook* ptr)
    {
        auto target = os_get_module_function(module, name);

        if (target == nullptr)
        {
            return REVERSE_STATUS_FUNCTION_NOT_FOUND;
        }

        return reverse_hook_function(target, hook, ptr);
    }

    bool reverse_enable_all_hooks()
    {
        log("reverse: Enabling all hooks\n");

        auto res = MH_EnableHook(MH_ALL_HOOKS);

        if (res != MH_OK)
        {
            log("Could not enable all hooks ('{}')\n", MH_StatusToString(res));
        }

        return res == MH_OK;
    }

    void reverse_init()
    {
        MH_Initialize();
    }

    void reverse_destroy()
    {
        MH_Uninitialize();
    }
}
