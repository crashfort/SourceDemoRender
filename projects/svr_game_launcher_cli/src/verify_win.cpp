#include <svr/log_format.hpp>
#include <svr/defer.hpp>
#include <svr/str.hpp>

#include <d3d11.h>
#include <stdint.h>

static bool verify_d3d11_features()
{
    using namespace svr;

    // Should be good enough for all the features that we make use of.
    const auto MINIMUM_VERSION = D3D_FEATURE_LEVEL_11_0;

    D3D_FEATURE_LEVEL levels[] = {
        MINIMUM_VERSION
    };

    ID3D11Device* device = nullptr;

    defer {
        if (device) device->Release();
    };

    D3D_FEATURE_LEVEL created_level;

    auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1, D3D11_SDK_VERSION, &device, &created_level, nullptr);

    if (FAILED(hr))
    {
        log("Could not create d3d11 device ({:#x})\n", (uint32_t)hr);
        return false;
    }

    // Only support the minimum feature level and nothing else.

    if (created_level < MINIMUM_VERSION)
    {
        log("Created device with feature level {} but minimum is {}\n", created_level, MINIMUM_VERSION);
        return false;
    }

    UINT bits = 0;

    // This format must be supported for typed UAV loads. It is the format that is used for motion blur.
    // It is a feature of D3D11.3 and above.
    device->CheckFormatSupport(DXGI_FORMAT_R32G32B32A32_FLOAT, &bits);

    if (!(bits & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW))
    {
        log("DXGI_FORMAT_R32G32B32A32_FLOAT not supported for typed UAV\n");
        return false;
    }

    return true;
}

bool verify_system_features()
{
    if (!verify_d3d11_features()) return false;

    return true;
}

bool verify_executable_name(const char* name)
{
    return svr::str_ends_with(name, ".exe");
}
