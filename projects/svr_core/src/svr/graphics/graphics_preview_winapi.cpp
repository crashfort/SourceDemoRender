#include <svr/graphics_preview.hpp>
#include <svr/graphics.hpp>
#include <svr/log_format.hpp>
#include <svr/defer.hpp>
#include <svr/swap.hpp>

#include <Windows.h>

#include <mutex>

static const auto PREVIEW_WINDOW_CLASS_NAME = "crashfort.svr.preview";

static LRESULT CALLBACK window_procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

// A window class only needs to be registered once.
static bool has_registered_window_class = false;

static bool register_window_class()
{
    if (has_registered_window_class)
    {
        return true;
    }

    auto instance = GetModuleHandleA(nullptr);

    WNDCLASSEX desc = {};
    desc.cbSize = sizeof(desc);
    desc.lpfnWndProc = window_procedure;
    desc.hInstance = instance;
    desc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    desc.lpszClassName = PREVIEW_WINDOW_CLASS_NAME;

    if (RegisterClassExA(&desc) == 0)
    {
        auto error = GetLastError();

        if (error != ERROR_CLASS_ALREADY_EXISTS)
        {
            svr::log("preview winapi: Could not register window class '{}' ({})\n", PREVIEW_WINDOW_CLASS_NAME, error);
            return false;
        }
    }

    has_registered_window_class = true;
    return true;
}

static HWND create_window(uint32_t width, uint32_t height)
{
    auto instance = GetModuleHandleA(nullptr);

    RECT rect = {};
    rect.right = width;
    rect.bottom = height;

    auto style = WS_OVERLAPPEDWINDOW;

    AdjustWindowRect(&rect, style, false);

    auto hwnd = CreateWindowExA(0, PREVIEW_WINDOW_CLASS_NAME, "SVR Preview", style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);

    if (hwnd == nullptr)
    {
        svr::log("preview winapi: Could not create preview window ({})\n", GetLastError());
        return false;
    }

    return hwnd;
}

static svr::graphics_swapchain* create_swapchain(svr::graphics_backend* graphics, uint32_t width, uint32_t height, void* hwnd)
{
    using namespace svr;

    graphics_swapchain_desc desc = {};
    desc.width = width;
    desc.height = height;
    desc.format = GRAPHICS_FORMAT_B8G8R8A8_UNORM;
    desc.window = hwnd;

    return graphics->create_swapchain("winapi preview swapchain", desc);
}

struct graphics_preview_winapi
    : svr::graphics_preview
{
    ~graphics_preview_winapi()
    {
        if (hwnd)
        {
            DestroyWindow(hwnd);
        }
    }

    void render(svr::graphics_srv* srv) override
    {
        hwnd_mutex.lock();
        render_to_hwnd(srv);
        hwnd_mutex.unlock();
    }

    void render_to_hwnd(svr::graphics_srv* srv)
    {
        using namespace svr;

        if (hwnd == nullptr)
        {
            return;
        }

        float clear_color[] = { 0.0f, 0.0f, 0.0f, 1.0f };

        auto window_rtv = graphics->get_swapchain_rtv(swapchain);
        graphics->clear_rtv(window_rtv, clear_color);

        // Read the new window size.

        viewport_mutex.lock();
        auto new_viewport = viewport;
        viewport_mutex.unlock();

        // Window size changed, resize swapchain.
        if (new_viewport.x != window_width || new_viewport.y != window_height)
        {
            window_width = new_viewport.x;
            window_height = new_viewport.y;

            graphics->resize_swapchain(swapchain, window_width, window_height);
        }

        graphics_overlay_desc overlay_desc;
        overlay_desc.rect = calc_aspect_ratio_rect(window_width, window_height, source_width, source_height);
        overlay_desc.sampler_state = GRAPHICS_SAMPLER_LINEAR;
        overlay_desc.blend_state = GRAPHICS_BLEND_OPAQUE;

        graphics->draw_overlay(srv, window_rtv, overlay_desc);
        graphics->present_swapchain(swapchain);
    }

    void prepare_window(uint32_t width, uint32_t height)
    {
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG>(this));

        source_width = width;
        source_height = height;

        window_width = width;
        window_height = height;

        viewport.x = width;
        viewport.y = height;

        // Make the window visible in the task bar but don't show it. Leave it minimized.
        ShowWindow(hwnd, SW_SHOWMINNOACTIVE);
    }

    svr::graphics_rect calc_aspect_ratio_rect(int w, int h, int source_w, int source_h)
    {
        svr::graphics_rect ret;

        auto window_aspect = (double)w / (double)h;
        auto base_aspect = (double)source_w / (double)source_h;

        if (window_aspect > base_aspect)
        {
            ret.w = (double)h* base_aspect;
            ret.h = h;
        }

        else
        {
            ret.w = w;
            ret.h = (float)w / base_aspect;
        }

        ret.x = w / 2 - ret.w / 2;
        ret.y = h / 2 - ret.h / 2;

        return ret;
    }

    std::mutex hwnd_mutex;
    HWND hwnd = nullptr;

    svr::graphics_backend* graphics = nullptr;
    svr::graphics_swapchain* swapchain = nullptr;

    // Actual dimensions of the texture that will be drawn.
    uint32_t source_width;
    uint32_t source_height;

    // Dimensions of the window.
    // Read from the viewport.
    uint32_t window_width;
    uint32_t window_height;

    std::mutex viewport_mutex;

    // Variable used from the message loop thread to set the fresh window size.
    // Will be compared against in the rendering function to resize the swapchain if needed.
    POINT viewport;
};

namespace svr
{
    graphics_preview* graphics_preview_create_winapi(graphics_backend* graphics, uint32_t width, uint32_t height)
    {
        if (!register_window_class())
        {
            log("preview winapi: Could not register window class\n");
            return nullptr;
        }

        auto hwnd = create_window(width, height);

        if (hwnd == nullptr)
        {
            log("preview winapi: Could not create window\n");
            return false;
        }

        defer {
            if (hwnd) DestroyWindow(hwnd);
        };

        auto swapchain = create_swapchain(graphics, width, height, hwnd);

        if (swapchain == nullptr)
        {
            log("preview winapi: Could not create swapchain\n");
            return false;
        }

        defer {
            if (swapchain) graphics->destroy_swapchain(swapchain);
        };

        auto prev = new graphics_preview_winapi;

        swap_ptr(prev->hwnd, hwnd);
        swap_ptr(prev->swapchain, swapchain);

        prev->graphics = graphics;

        prev->prepare_window(width, height);

        return prev;
    }

    void graphics_preview_destroy_winapi(graphics_preview* prev)
    {
        delete prev;
    }
}

static graphics_preview_winapi* transform_ptr(HWND hwnd)
{
    return reinterpret_cast<graphics_preview_winapi*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
}

static LRESULT CALLBACK window_procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        case WM_CLOSE:
        {
            // The default window procedure will destroy the window.
            // Remove our reference to the window.
            auto ptr = transform_ptr(hwnd);

            ptr->hwnd_mutex.lock();
            ptr->hwnd = nullptr;
            ptr->hwnd_mutex.unlock();

            break;
        }

        case WM_SIZE:
        {
            auto ptr = transform_ptr(hwnd);

            auto width = LOWORD(lparam);
            auto height = HIWORD(lparam);

            if (width != 0 && height != 0)
            {
                ptr->viewport_mutex.lock();
                ptr->viewport.x = width;
                ptr->viewport.y = height;
                ptr->viewport_mutex.unlock();
            }

            return 0;
        }
    }

    return DefWindowProcA(hwnd, message, wparam, lparam);
}
