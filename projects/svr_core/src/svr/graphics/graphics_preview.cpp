#include <svr/graphics_preview.hpp>

namespace svr
{
    void graphics_preview_destroy(graphics_preview* prev)
    {
        delete prev;
    }
}
