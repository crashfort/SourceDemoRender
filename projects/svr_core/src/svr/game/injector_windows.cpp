#include <svr/launcher.hpp>

#include <svr/log_format.hpp>
#include <svr/os.hpp>
#include <svr/os_win.hpp>

#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// The structure that will be located in the started process.
// It is used as a parameter to the below function bytes.
struct interprocess_structure
{
    // Windows API functions.
    decltype(LoadLibraryA)* load_library;
    decltype(GetProcAddress)* get_proc_address;
    decltype(GetLastError)* get_last_error;
    decltype(SetDllDirectoryA)* set_library_directory;

    // The name of the library to load.
    const char* library_name;

    // The initialization export function to call.
    const char* export_name;

    // The path of the SVR resource directory.
    const char* resource_path;

    // The identifer of the game as specified in the game config.
    const char* game_id;
};

// This is the function that will be injected into the target process.
// The instructions remote_func_bytes below is the result of this function.
static VOID CALLBACK remote_func(ULONG_PTR param)
{
    auto data = (interprocess_structure*)param;

    // There is no error handling here as there's no practical way to report
    // stuff back within this limited environment.
    // There have not been cases of these api functions failing with proper input
    // so let's stick with the simplest working solution for now.

    // Add our resource path as searchable to resolve library dependencies.
    data->set_library_directory(data->resource_path);

    auto module = data->load_library(data->library_name);
    auto func = (svr::svr_init_type)data->get_proc_address(module, data->export_name);

    svr::launcher_init_data init;
    init.resource_path = data->resource_path;
    init.game_id = data->game_id;

    func(init);

    // Restore the default search order.
    data->set_library_directory(nullptr);
}

// The code that will run in the started process.
// It is responsible of injecting our library into itself.
static uint8_t remote_func_bytes[] =
{
    0x55,
    0x8b, 0xec,
    0x83, 0xec, 0x10,
    0x56,
    0x8b, 0x75, 0x08,
    0xff, 0x76, 0x18,
    0x8b, 0x46, 0x0c,
    0xff, 0xd0,
    0xff, 0x76, 0x10,
    0x8b, 0x06,
    0xff, 0xd0,
    0xff, 0x76, 0x14,
    0x8b, 0x4e, 0x04,
    0x50,
    0xff, 0xd1,
    0x8b, 0x4e, 0x18,
    0x89, 0x4d, 0xf4,
    0x8b, 0x4e, 0x1c,
    0x89, 0x4d, 0xf8,
    0x8d, 0x4d, 0xf4,
    0x51,
    0xff, 0xd0,
    0x8b, 0x46, 0x0c,
    0x83, 0xc4, 0x04,
    0x6a, 0x00,
    0xff, 0xd0,
    0x5e,
    0x8b, 0xe5,
    0x5d,
    0xc2, 0x04, 0x00,
};

// Use this to generate new content for remote_code_bytes in case remote_func changes.
static int simulate()
{
    interprocess_structure structure = {};
    structure.load_library = LoadLibraryA;
    structure.get_proc_address = GetProcAddress;
    structure.get_last_error = GetLastError;
    structure.set_library_directory = SetDllDirectoryA;

    // It is important to use QueueUserAPC here to produce the correct output.
    // Calling remote_func directly will produce uniquely optimized code which cannot
    // work in another process.
    QueueUserAPC(remote_func, GetCurrentThread(), (ULONG_PTR)&structure);

    // Used to signal the thread so the queued function will run.
    SleepEx(0, 1);

    return 0;
}

bool inject_process(svr::os_handle* proc, svr::os_handle* thread, const char* resource_path, const char* game_id)
{
    using namespace svr;

    log("Allocating 4096 remote bytes\n");

    // Allocate a sufficient enough size in the target process.
    // It needs to be able to contain all function bytes and the structure
    // containing variable length strings.
    // The virtual memory that we allocated should not be freed as it will be used
    // as reference for future use within the application itself.
    auto mem = os_alloc_remote_proc_mem(proc, 4096);

    if (mem == nullptr)
    {
        log("Could not remotely allocate process memory\n");
        return false;
    }

    log("Remote address: {}\n", mem);

    // Use a writer to append everything sequentially.
    auto writer = os_remote_proc_mem_writer(proc, mem);

    auto remote_func_addr = writer.write_region(remote_func_bytes, sizeof(remote_func_bytes));

    // All addresses here must match up in the context of the target process, not our own.
    // The operating system api functions will always be located in the same address of every
    // process so those do not have to be adjusted.
    interprocess_structure structure;
    structure.load_library = LoadLibraryA;
    structure.get_proc_address = GetProcAddress;
    structure.get_last_error = GetLastError;
    structure.set_library_directory = SetDllDirectoryA;
    structure.library_name = writer.write_string("svr_game.dll");
    structure.export_name = writer.write_string("svr_init");
    structure.resource_path = writer.write_string(resource_path);
    structure.game_id = writer.write_string(game_id);

    auto remote_structure_addr = writer.write_region(&structure, sizeof(structure));

    // Queue up our procedural function to run instantly on the main thread when the process is resumed.
    if (!os_queue_async_thread_func(thread, remote_func_addr, remote_structure_addr))
    {
        log("Could not queue async thread func for injection\n");
        return false;
    }

    return true;
}
