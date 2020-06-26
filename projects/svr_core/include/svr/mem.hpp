#pragma once
#include <svr/api.hpp>

#include <stdint.h>

namespace svr
{
    struct mem_buffer
    {
        void* data = nullptr;
        size_t size = 0;
    };

    // Allocates uninitialized memory.
    SVR_API bool mem_create_buffer(mem_buffer& buf, size_t size);

    // Frees previously allocated memory.
    SVR_API void mem_destroy_buffer(mem_buffer& buf);
}
