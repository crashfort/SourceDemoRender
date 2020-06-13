#pragma once
#include <svr/api.hpp>
#include <svr/vec.hpp>

namespace svr
{
    struct config_node;

    struct graphics_backend;
    struct graphics_rtv;
    struct graphics_texture;
    struct graphics_text_format;

    enum graphics_velocity_element_axes
    {
        GRAPHICS_VELOCITY_ELEMENT_XY,
        GRAPHICS_VELOCITY_ELEMENT_XYZ,
    };

    // Display element that displays the velocity of a particular player.
    // Renders text to the parented render target.
    class graphics_velocity_element
    {
    public:
        graphics_velocity_element(graphics_backend* graphics, graphics_rtv* rtv, graphics_text_format* text_format);

        ~graphics_velocity_element();

        // Sets the display mode format.
        void set_axes(graphics_velocity_element_axes value);

        // Sets how many velocity samples to collect before updating.
        void set_buffer(int value);

        // Updates the state of the element.
        // May write to the parented render target.
        void update(const vec3& velocity);

    private:
        int buffer;
        int next_update;
        vec3 velocity;

        graphics_velocity_element_axes axes;

        graphics_backend* graphics;
        graphics_rtv* rtv;

        graphics_text_format* text_format;
    };

    // Reads a configuration node and applies the parameters to the velocity text element.
    // Returns false if a node is missing in the configuration.
    // Returns true if the state was changed.
    SVR_API bool graphics_velocity_element_load_configuration(graphics_velocity_element* ptr,
                                                              const config_node& node);
}
