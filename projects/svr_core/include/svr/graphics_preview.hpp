#pragma once
#include <svr/api.hpp>

#include <stdint.h>

namespace svr
{
    struct graphics_srv;
    struct graphics_backend;

    struct graphics_preview
    {
        virtual ~graphics_preview() = default;

        // Renders the provided texture view to the preview window.
        // Should be a texture that has equal size as the source dimensions.
        virtual void render(graphics_srv* srv) = 0;
    };

    // Creates a preview window.
    // Input sizes are the dimensions of the source material.
    // A message loop must be created in the same thread afterwards.
    SVR_API graphics_preview* graphics_preview_create_winapi(graphics_backend* graphics, uint32_t width, uint32_t height, bool minimized);

    // Destroys a preview window.
    // Should be called in the same thread that created the preview window and also ran the message loop.
    // A preview window should be destroyed when the message loop exits.
    SVR_API void graphics_preview_destroy(graphics_preview* prev);
}
