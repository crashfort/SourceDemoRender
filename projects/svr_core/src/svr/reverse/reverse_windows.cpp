#include <svr/reverse.hpp>

namespace svr
{
    // Virtual tables are placed at the start in MSVC.
    void* reverse_get_virtual(void* ptr, int index)
    {
        auto vtable = *((void***)ptr);
        auto address = vtable[index];

        return address;
    }
}
