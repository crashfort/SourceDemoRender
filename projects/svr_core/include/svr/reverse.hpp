#pragma once
#include <svr/api.hpp>

#include <stdint.h>

namespace svr
{
    struct os_module;

    enum reverse_status
    {
        REVERSE_STATUS_OK,
        REVERSE_STATUS_ALREADY_CREATED,
        REVERSE_STATUS_NOT_CREATED,
        REVERSE_STATUS_NOT_EXECUTABLE,
        REVERSE_STATUS_FUNCTION_NOT_FOUND,
        REVERSE_STATUS_FUNCTION_NOT_SUPPORTED
    };

    struct reverse_hook
    {
        // Address of the function to be targetted.
        void* target;

        // Address of the function that is to replace the target.
        void* hook;

        // Address of the trampoline location used to call the original function.
        void* original;
    };

    template <typename FuncT>
    struct reverse_hook_template
        : reverse_hook
    {
        auto get_original() const
        {
            return (FuncT)original;
        }
    };

    // Initializes the reverse system.
    // This must be called before any reverse functions can be used.
    SVR_API void reverse_init();

    // Destroys the reverse systems.
    SVR_API void reverse_destroy();

    // Hooks a function by address.
    // Hooks must be enabled before anything is commited.
    SVR_API reverse_status reverse_hook_function(void* target, void* hook, reverse_hook* ptr);

    // Enables all hooks.
    SVR_API bool reverse_enable_all_hooks();

    // Finds a memory address from a pattern in a range.
    // Format should be like '8B 0D ?? ?? ?? ?? 56'.
    // Unknown bytes should be denoted as '??'.
    SVR_API void* reverse_find_pattern(void* base, size_t size, const char* pattern);

    // Applies an offset to an address and returns the new address.
    SVR_API void* reverse_add_offset(void* address, ptrdiff_t offset);

    // Follows a relative jump instruction.
    SVR_API void* reverse_follow_rel_jump(void* address);

    // Returns a virtual function from a virtual table.
    SVR_API void* reverse_get_virtual(void* ptr, int index);
}
