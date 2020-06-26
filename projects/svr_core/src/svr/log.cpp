#include <svr/log.hpp>

static svr::log_function_type log_func;
static void* log_context;

namespace svr
{
    void log(const char* text)
    {
        if (log_func)
        {
            log_func(log_context, text);
        }
    }

    void log_set_function(log_function_type func, void* context)
    {
        log_func = func;
        log_context = context;
    }

    bool log_enabled()
    {
        return log_func != nullptr;
    }
}
