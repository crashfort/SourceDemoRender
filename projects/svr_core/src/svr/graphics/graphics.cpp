#include <svr/graphics.hpp>

namespace svr
{
    // Destroys an instance of a graphics backend.
    void graphics_destroy_backend(graphics_backend* ptr)
    {
        delete ptr;
    }
}
