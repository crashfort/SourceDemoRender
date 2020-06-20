#include <svr/os.hpp>

namespace svr
{
    void os_normalize_path(char* buf, size_t size)
    {
        for (size_t i = 0; i < size; i++)
        {
            if (*buf == '\\')
            {
                *buf = '/';
            }

            buf++;
        }
    }
}
