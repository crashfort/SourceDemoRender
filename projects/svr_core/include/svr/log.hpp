#pragma once
#include <svr/api.hpp>

namespace svr
{
    using log_function_type = void(*)(void* context, const char* text);

    // Forwards text to the provided function handler.
    // The provided function is required to handle all synchronization.
    // Does not do anything if logging is disabled.
    SVR_API void log(const char* text);

    // Sets the function to handle logging requests.
    // Set an empty function to disable logging.
    // There is no protection for adjusting this global state.
    SVR_API void log_set_function(log_function_type func, void* context);

    // Returns whether or not logging is enabled.
    // This is decided purely from there being a function handler or not.
    // There is no protection for adjusting this global state.
    SVR_API bool log_enabled();
}
