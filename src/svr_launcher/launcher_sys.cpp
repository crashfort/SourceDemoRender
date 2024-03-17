#include "launcher_priv.h"

void LauncherState::sys_show_windows_version()
{
    HKEY hkey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hkey) != 0)
    {
        return;
    }

    const s32 REG_BUF_SIZE = 64;

    char product_name[REG_BUF_SIZE];
    char current_build[REG_BUF_SIZE];
    char release_id[REG_BUF_SIZE];

    DWORD product_name_size = REG_BUF_SIZE;
    DWORD current_build_size = REG_BUF_SIZE;
    DWORD release_id_size = REG_BUF_SIZE;

    RegGetValueA(hkey, NULL, "ProductName", RRF_RT_REG_SZ, NULL, product_name, &product_name_size);
    RegGetValueA(hkey, NULL, "CurrentBuild", RRF_RT_REG_SZ, NULL, current_build, &current_build_size);
    RegGetValueA(hkey, NULL, "ReleaseId", RRF_RT_REG_SZ, NULL, release_id, &release_id_size);

    // Will show like Windows 10 Enterprise version 2004 build 19041.
    char winver[192];
    SVR_SNPRINTF(winver, "%s version %d build %d", product_name, atoi(release_id), strtol(current_build, NULL, 10));

    svr_log("Using operating system %s\n", winver);
}

void trim_right(char* buf, s32 length)
{
    s32 len = length;
    char* start = buf;
    char* end = buf + len - 1;

    while (end != start && svr_is_whitespace(*end))
    {
        end--;
    }

    end++;
    *end = 0;
}

void LauncherState::sys_show_processor()
{
    HKEY hkey;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hkey) != 0)
    {
        return;
    }

    const s32 REG_BUF_SIZE = 128;

    char name[REG_BUF_SIZE];
    DWORD name_size = REG_BUF_SIZE;

    RegGetValueA(hkey, NULL, "ProcessorNameString", RRF_RT_REG_SZ, NULL, name, &name_size);

    // The value will have a lot of extra spaces at the end.
    trim_right(name, strlen(name));

    svr_log("Using processor %s (%lu cpus)\n", name, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
}

void LauncherState::sys_show_available_memory()
{
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);

    GlobalMemoryStatusEx(&mem);

    svr_log("The system has %lld mb of memory (%lld mb usable)\n", SVR_FROM_MB(mem.ullTotalPhys), SVR_FROM_MB(mem.ullAvailPhys));
}

// We cannot store this result so it has to be done every start.
void LauncherState::sys_check_hw_caps()
{
    ID3D11Device* d3d11_device = NULL;
    ID3D11DeviceContext* d3d11_context = NULL;

    UINT device_create_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

    // Use a lower feature level here than needed (we actually use 12_0) in order to get a better description
    // of the adapter below, and also to more accurately query the hw caps.
    const D3D_FEATURE_LEVEL MINIMUM_DEVICE_LEVEL = D3D_FEATURE_LEVEL_11_0;

    const D3D_FEATURE_LEVEL DEVICE_LEVELS[] = {
        MINIMUM_DEVICE_LEVEL
    };

    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, device_create_flags, DEVICE_LEVELS, 1, D3D11_SDK_VERSION, &d3d11_device, NULL, &d3d11_context);

    if (FAILED(hr))
    {
        svr_log("D3D11CreateDevice failed with code %#x\n", hr);
        launcher_error("HW support could not be queried. Is there a graphics adapter in the system?");
    }

    IDXGIDevice* dxgi_device;
    d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgi_device));

    IDXGIAdapter* dxgi_adapter;
    dxgi_device->GetAdapter(&dxgi_adapter);

    DXGI_ADAPTER_DESC dxgi_adapter_desc;
    dxgi_adapter->GetDesc(&dxgi_adapter_desc);

    // Useful for future troubleshooting.
    // Use https://www.pcilookup.com/ to see more information about device and vendor ids.
    svr_log("Using graphics device %x by vendor %x\n", dxgi_adapter_desc.DeviceId, dxgi_adapter_desc.VendorId);

    D3D11_FEATURE_DATA_FORMAT_SUPPORT2 fmt_support2;
    fmt_support2.InFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
    d3d11_device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &fmt_support2, sizeof(D3D11_FEATURE_DATA_FORMAT_SUPPORT2));

    bool has_typed_uav_load = fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD;
    bool has_typed_uav_store = fmt_support2.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_STORE;
    bool has_typed_uav_support = has_typed_uav_load && has_typed_uav_store;

    if (!has_typed_uav_support)
    {
        launcher_error("This system does not meet the requirements to use SVR.");
    }
}
