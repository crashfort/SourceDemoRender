#include <svr/mem.hpp>
#include <svr/log_format.hpp>

#include <malloc.h>

namespace svr
{
    bool mem_create_buffer(mem_buffer& buf, size_t size)
    {
        auto data = malloc(size);

        if (data == nullptr)
        {
            log("mem: Could not allocate {} bytes\n", size);
            return false;
        }

        buf.data = data;
        buf.size = size;
        return true;
    }

    void mem_destroy_buffer(mem_buffer& buf)
    {
        if (buf.data)
        {
            free(buf.data);

            buf.data = nullptr;
            buf.size = 0;
        }
    }
}
