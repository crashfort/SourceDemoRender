#include <svr/graphics_preview.hpp>

// Selects to use the winapi preview for Windows.

svr::graphics_preview* create_system_preview(svr::graphics_backend* graphics, uint32_t width, uint32_t height)
{
    return svr::graphics_preview_create_winapi(graphics, width, height);
}
