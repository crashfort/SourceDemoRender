#pragma once
#include <svr/api.hpp>
#include <svr/media.hpp>

#include <stdint.h>

namespace svr
{
    struct os_handle;

    enum graphics_blend_state_type
    {
        GRAPHICS_BLEND_OPAQUE,
        GRAPHICS_BLEND_ALPHA_BLEND,
        GRAPHICS_BLEND_ADDITIVE,
        GRAPHICS_BLEND_NONPREMULTIPLIED,
    };

    enum graphics_sampler_state_type
    {
        GRAPHICS_SAMPLER_POINT,
        GRAPHICS_SAMPLER_LINEAR,
    };

    enum graphics_format
    {
        GRAPHICS_FORMAT_B8G8R8A8_UNORM,
        GRAPHICS_FORMAT_R8G8B8A8_UINT,
        GRAPHICS_FORMAT_R8G8B8A8_UNORM,
        GRAPHICS_FORMAT_R32G32B32A32_FLOAT,
        GRAPHICS_FORMAT_R8_UNORM,
        GRAPHICS_FORMAT_R8_UINT,
        GRAPHICS_FORMAT_R8G8_UINT,
        GRAPHICS_FORMAT_R32_UINT,
    };

    enum graphics_buffer_type
    {
        GRAPHICS_BUFFER_VERTEX,
        GRAPHICS_BUFFER_INDEX,
        GRAPHICS_BUFFER_CONSTANT,
        GRAPHICS_BUFFER_GENERIC,
    };

    enum graphics_shader_type
    {
        GRAPHICS_SHADER_PIXEL,
        GRAPHICS_SHADER_VERTEX,
        GRAPHICS_SHADER_COMPUTE,
    };

    enum graphics_resource_usage
    {
        GRAPHICS_USAGE_DEFAULT,
        GRAPHICS_USAGE_IMMUTABLE,
        GRAPHICS_USAGE_DYNAMIC,
        GRAPHICS_USAGE_STAGING,
    };

    enum graphics_view_access
    {
        GRAPHICS_VIEW_NONE = 0,
        GRAPHICS_VIEW_SRV = 1u << 0,
        GRAPHICS_VIEW_UAV = 1u << 1,
        GRAPHICS_VIEW_RTV = 1u << 2,
    };

    using graphics_view_access_t = uint32_t;

    enum graphics_cpu_access
    {
        GRAPHICS_CPU_ACCESS_NONE = 0,
        GRAPHICS_CPU_ACCESS_READ = 1u << 0,
        GRAPHICS_CPU_ACCESS_WRITE = 1u << 1,
    };

    using graphics_cpu_access_t = uint32_t;

    enum graphics_resource_cap
    {
        GRAPHICS_CAP_NONE = 0,

        // Capability that creates an additional mirrored resource
        // to allow fast downloading without stalling.
        // Can not be used with constant buffers.
        GRAPHICS_CAP_DOWNLOADABLE = 1u << 0,
    };

    using graphics_resource_cap_t = uint32_t;

    struct graphics_rect
    {
        int x;
        int y;
        int w;
        int h;
    };

    struct graphics_initial_desc
    {
        const void* data;
        size_t size;
    };

    struct graphics_texture_desc
    {
        uint32_t width;
        uint32_t height;
        graphics_format format;
        graphics_resource_usage usage;
        graphics_view_access_t view_access;
        graphics_cpu_access_t cpu_access;
        graphics_resource_cap_t caps;

        graphics_initial_desc initial_desc;

        bool text_target;
    };

    struct graphics_texture_load_desc
    {
        graphics_resource_usage usage;
        graphics_view_access_t view_access;
        graphics_cpu_access_t cpu_access;
        graphics_resource_cap_t caps;
    };

    struct graphics_texture_open_desc
    {
        graphics_view_access_t view_access;
        graphics_resource_cap_t caps;
    };

    struct graphics_buffer_view_desc
    {
        graphics_format format;
        size_t elements;
    };

    struct graphics_buffer_desc
    {
        size_t size;
        graphics_resource_usage usage;
        graphics_buffer_type type;
        graphics_view_access_t view_access;
        graphics_cpu_access_t cpu_access;
        graphics_resource_cap_t caps;

        graphics_initial_desc initial_desc;

        graphics_buffer_view_desc view_desc;
    };

    struct graphics_swapchain_desc
    {
        uint32_t width;
        uint32_t height;
        graphics_format format;
        void* window;
    };

    struct graphics_shader_desc
    {
        graphics_shader_type type;
        const void* data;
        size_t size;
    };

    struct graphics_overlay_desc
    {
        // The rasterization rectangle in the render target.
        graphics_rect rect;

        graphics_sampler_state_type sampler_state;
        graphics_blend_state_type blend_state;
    };

    struct graphics_motion_sample_desc
    {
        // The dimensions of the textures that are intended to be sampled.
        uint32_t width;
        uint32_t height;
    };

    struct graphics_conversion_context_desc
    {
        // The dimensions of the textures that are intended to be converted.
        // The input dimensions must match the output dimensions.
        uint32_t width;
        uint32_t height;

        // The texture format that input will be in.
        graphics_format source_format;

        // The image format that the output will be in.
        media_pixel_format dest_format;

        // The color space that output will be in.
        media_color_space dest_color_space;
    };

    struct graphics_text_format_desc
    {
        // The name of a font family that is installed in the system.
        const char* font_family;

        // The point height size of the font to use.
        uint32_t font_size;

        // The color to use when drawing this text.
        uint8_t color_r;
        uint8_t color_g;
        uint8_t color_b;
        uint8_t color_a;

        // Possible values:
        // normal, oblique, italic.
        // defaults to normal if empty.
        const char* font_style;

        // Possible values:
        // thin, extralight, light, semilight, normal, medium,
        // semibold, bold, extrabold, black, extrablack.
        // Defaults to normal if empty.
        const char* font_weight;

        // Possible values:
        // undefined, ultracondensed, extracondensed, condensed, semicondensed,
        // normal, semiexpanded, expanded, extraexpanded, ultraexpanded.
        // Defaults to normal if empty.
        const char* font_stretch;

        // Possible values:
        // leading, trailing, center.
        // Defaults to center if empty.
        const char* text_align;

        // Possible values:
        // near, far, center.
        // Defaults to center if empty.
        const char* paragraph_align;
    };

    struct graphics_backend;
    struct graphics_shader;
    struct graphics_srv;
    struct graphics_rtv;
    struct graphics_uav;
    struct graphics_texture;
    struct graphics_buffer;
    struct graphics_swapchain;
    struct graphics_blend_state;
    struct graphics_sampler_state;
    struct graphics_conversion_context;
    struct graphics_text_format;

    struct graphics_backend
    {
        virtual ~graphics_backend() = default;

        // Creates a swap chain that is aimed towards a window.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_swapchain* create_swapchain(const char* name, const graphics_swapchain_desc& desc) = 0;

        // Destroys a swap chain.
        virtual void destroy_swapchain(graphics_swapchain* ptr) = 0;

        // Presents the contents of a swap chain render target to the display.
        // Does not wait for vertical sync.
        virtual void present_swapchain(graphics_swapchain* ptr) = 0;

        // Resizes a swap chain to a new size.
        virtual void resize_swapchain(graphics_swapchain* ptr, uint32_t width, uint32_t height) = 0;

        // Returns the render target view of a swap chain.
        virtual graphics_rtv* get_swapchain_rtv(graphics_swapchain* ptr) = 0;

        // Creates a new texture.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_texture* create_texture(const char* name, const graphics_texture_desc& desc) = 0;

        // Creates a new texture from a file.
        // This function accesses the file system and may block for a considerable amount of time.
        // Always creates textures in R8G8B8A8 format.
        // Supports all common image formats.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_texture* create_texture_from_file(const char* name, const char* file, const graphics_texture_load_desc& desc) = 0;

        // Opens up a shared texture resource that lives in another graphics api, such as d3d9ex.
        // The texture must have been created as a shared resource.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_texture* open_shared_texture(const char* name, os_handle* handle, const graphics_texture_open_desc& desc) = 0;

        // Destroys a previously created texture.
        // Resources can be destroyed manually if needed.
        // All resoruces are destroyed when the backend is destroyed.
        virtual void destroy_texture(graphics_texture* ptr) = 0;

        // Returns the shader resource view of a texture, if it has one.
        virtual graphics_srv* get_texture_srv(graphics_texture* ptr) = 0;

        // Returns the render target view of a texture, if it's a render target.
        virtual graphics_rtv* get_texture_rtv(graphics_texture* ptr) = 0;

        // Returns the unordered access view of a texture, if it has one.
        virtual graphics_uav* get_texture_uav(graphics_texture* ptr) = 0;

        virtual void copy_texture(graphics_texture* source, graphics_texture* dest) = 0;

        // Clears a render target view with normalized color values.
        virtual void clear_rtv(graphics_rtv* value, float color[4]) = 0;

        // Returns the total size in bytes of a texture.
        // This size can be used to create a buffer used for downloading a texure.
        virtual size_t get_texture_size(graphics_texture* value) = 0;

        // Creates a new buffer.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_buffer* create_buffer(const char* name, const graphics_buffer_desc& desc) = 0;

        // Destroys a previously created buffer.
        // Resources can be destroyed manually if needed.
        // All resoruces are destroyed when the backend is destroyed.
        virtual void destroy_buffer(graphics_buffer* ptr) = 0;

        // Returns the shader resource view of a buffer, if it has one.
        virtual graphics_srv* get_buffer_srv(graphics_buffer* ptr) = 0;

        // Returns the unordered access view of a buffer, if it has one.
        virtual graphics_uav* get_buffer_uav(graphics_buffer* ptr) = 0;

        // Returns the total size in bytes of a buffer.
        // This size can be used to create a buffer used for downloading a buffer.
        virtual size_t get_buffer_size(graphics_buffer* value) = 0;

        // Draws an overlay to a render target.
        virtual void draw_overlay(graphics_srv* source, graphics_rtv* dest, const graphics_overlay_desc& desc) = 0;

        // Applies a weighted overlay on top of an existing texture.
        // Can be used to create motion blur with enough samples.
        // The SRV and UAV resources should be based on the same format.
        // The UAV is modified in place.
        virtual void motion_sample(const graphics_motion_sample_desc& desc, graphics_srv* source, graphics_uav* dest, float weight) = 0;

        // Creates a conversion context.
        // The conversion context will contain as many buffers as necessary for the destination format.
        // The buffers will be created as downloadable buffers.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_conversion_context* create_conversion_context(const char* name, const graphics_conversion_context_desc& desc) = 0;

        // Destroys a conversion context.
        virtual void destroy_conversion_context(graphics_conversion_context* ptr) = 0;

        // Converts a texture into a series of textures depending on the conversion context.
        // The resulting textures can then be downloaded to get into system memory.
        virtual void convert_pixel_formats(graphics_srv* source, graphics_conversion_context* dest, graphics_texture** res, size_t size) = 0;

        // Returns how many textures are being used by a conversion context.
        virtual size_t get_conversion_texture_count(graphics_conversion_context* ptr) = 0;

        // Returns the sizes in bytes for each conversion texture.
        virtual void get_conversion_sizes(graphics_conversion_context* ptr, size_t* sizes, size_t count) = 0;

        // Downloads a texture into system memory.
        // The texture is required to be downloadable.
        // The size is assumed to be known beforehand and is not checked in this function.
        virtual void download_texture(graphics_texture* source, void* dest, size_t size) = 0;

        // Downloads a buffer into system memory.
        // The buffer is required to be downloadable.
        // The size is assumed to be known beforehand and is not checked in this function.
        virtual void download_buffer(graphics_buffer* source, void* dest, size_t size) = 0;

        // Creates a text format from a description.
        // The provided name should be of static storage duration.
        // The returned value must be destroyed.
        virtual graphics_text_format* create_text_format(const char* name, graphics_texture* tex, const graphics_text_format_desc& desc) = 0;

        // Destroys a text format.
        virtual void destroy_text_format(graphics_text_format* ptr) = 0;

        // Draws text to the parented render target.
        // The text is not cached for future calls.
        // The coordinates form the text rectangle bounds.
        virtual void draw_text(graphics_text_format* ptr, const char* text, int left, int top, int right, int bottom) = 0;
    };

    // Creates an instance of a graphics backend.
    SVR_API graphics_backend* graphics_create_d3d11_backend(const char* resource_path);

    // Destroys an instance of a graphics backend.
    SVR_API void graphics_destroy_backend(graphics_backend* ptr);
}
