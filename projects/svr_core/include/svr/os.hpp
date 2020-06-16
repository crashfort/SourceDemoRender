#pragma once
#include <svr/api.hpp>

#include <stdint.h>
#include <string.h>

namespace svr
{
    struct mem_buffer;

    struct os_handle;
    struct os_module;

    // Structure representing a loaded process module.
    struct os_module_info
    {
        void* base;
        size_t size;
    };

    struct os_start_proc_desc
    {
        bool suspended;
        bool hide_window;
        os_handle* input_pipe;
    };

    // Creates a synchronization event that is shared between processes.
    // The event is by default not signalled.
    SVR_API os_handle* os_create_event(const char* name);

    // Attempts to open a previously created process shared synchronization event.
    SVR_API os_handle* os_open_event(const char* name);

    // Closes a generic handle.
    SVR_API void os_close_handle(os_handle* ptr);

    // Sets a process shared synchronization event to a signalled state.
    SVR_API void os_set_event(os_handle* ptr);

    // Resets a process shared synchronization event to a nonsignalled state.
    SVR_API void os_reset_event(os_handle* ptr);

    // Waits for any handle to become signalled.
    // Returns the handle which got signalled.
    // Timeout is in milliseconds, use -1 for infinite.
    SVR_API os_handle* os_handle_wait_any(os_handle** handles, size_t size, uint32_t timeout);

    // Waits for all handles to become signalled.
    // Timeout is in milliseconds, use -1 for infinite.
    SVR_API void os_handle_wait_all(os_handle** handles, size_t size, uint32_t timeout);

    inline bool os_handle_wait(os_handle* value, uint32_t timeout)
    {
        os_handle* waitables[] = {
            value
        };

        return os_handle_wait_any(waitables, 1, timeout) == value;
    }

    inline os_handle* os_handle_wait_either(os_handle* first, os_handle* second, uint32_t timeout)
    {
        os_handle* waitables[] = {
            first,
            second
        };

        return os_handle_wait_any(waitables, 2, timeout);
    }

    SVR_API os_handle* os_open_mutex(const char* name);

    SVR_API void os_release_mutex(os_handle* ptr);

    // Attempts to read a filesystem file into memory.
    // The file is read all in one go.
    // On failure, the resulting memory buffer is unchanged.
    SVR_API bool os_read_file(const char* path, mem_buffer& buffer);

    // Attempts to write data to a file.
    // The data is written all in one go.
    SVR_API bool os_write_file(const char* path, const void* data, size_t size);

    // Creates a new pipe endpoint for reading.
    SVR_API os_handle* os_create_pipe_read(const char* name);

    // Connects to a pipe endpoint for writing.
    SVR_API os_handle* os_open_pipe_write(const char* name);

    // Creates an unnamed pipe that can be used for writing to.
    // Suitable for writing to a process stdin.
    SVR_API bool os_create_pipe_pair(os_handle** read, os_handle** write);

    // Reads some bytes from a pipe.
    // Can only be called on the pipe endpoint made for reading.
    // Will block if there's no pending data and the writing end is not sending anything.
    SVR_API bool os_read_pipe(os_handle* ptr, void* data, size_t size, size_t* read);

    // Writes some bytes to a pipe.
    // Can only be called on the pipe endpoint made for writing.
    SVR_API bool os_write_pipe(os_handle* ptr, const void* data, size_t size);

    // Retrieves module information from a specified module name.
    SVR_API bool os_get_module_info(const char* name, os_module_info* ptr);

    // Returns a loaded module from a name.
    SVR_API os_module* os_get_module(const char* name);

    // Returns the address of an exported library function.
    SVR_API void* os_get_module_function(os_module* mod, const char* name);

    // Retrieves the name of a module.
    // This only writes the file name and nothing else.
    // The buffer will be null terminated and truncated if necessary.
    SVR_API bool os_get_module_name(os_handle* proc, os_module* mod, char* buf, size_t size);

    // Creates a process in a suspended state.
    // The main thread in the created process is halted.
    SVR_API bool os_start_proc(const char* exe, const char* dir, const char* args, const os_start_proc_desc* desc, os_handle** proc_handle, os_handle** thread_handle);

    // Attempts to open a process from a given identifier.
    SVR_API bool os_open_proc(uint64_t id, os_handle** proc_handle);

    // Returns the process id.
    SVR_API uint64_t os_get_proc_id(os_handle* ptr);

    // Returns the current process id.
    SVR_API uint64_t os_get_proc_id_self();

    // Returns the identifier of the main thread.
    SVR_API uint64_t os_get_thread_id(os_handle* ptr);

    // Returns the identifier of the current thread.
    SVR_API uint64_t os_get_thread_id_self();

    // Returns the active process handle.
    SVR_API os_handle* os_get_proc_handle_self();

    // Returns a process identifier from a thread handle.
    SVR_API uint64_t os_get_proc_id_from_thread(os_handle* ptr);

    // Resumes the execution of a thread.
    // Only does anything if it was suspended in the first place.
    SVR_API void os_resume_thread(os_handle* ptr);

    // Terminates and forcefully closes the specified process.
    SVR_API void os_terminate_proc(os_handle* ptr);

    // Terminates the active process.
    SVR_API void os_terminate_proc_self();

    // Allocates some executable memory in the specified process.
    // Returns the virtual address that corresponds to the allocated memory.
    // The returned address is not usable in the context of the calling process.
    SVR_API void* os_alloc_remote_proc_mem(os_handle* ptr, size_t size);

    // Frees some virtual memory in the specified process.
    SVR_API void os_free_remote_proc_mem(os_handle* ptr, void* target);

    // Writes some memory in the specified process.
    // The target memory address must be allocated in the specified process.
    // Output parmeter is optional.
    SVR_API bool os_write_remote_proc_mem(os_handle* ptr, void* target, const void* source, size_t size, size_t* written);

    // Reads some memory in the specified process.
    SVR_API bool os_read_remote_proc_mem(os_handle* ptr, const void* source, void* dest, size_t size, size_t* read);

    // Queries a process for all of its loaded modules.
    // The list will be written with all the loaded modules.
    SVR_API bool os_query_proc_modules(os_handle* ptr, os_module** list, size_t size, size_t* written);

    // Checks if a list of module names are loaded within a process.
    // Input should be a list of module names with no paths.
    // Returns how many modules that are loaded in the list.
    SVR_API size_t os_check_proc_modules(os_handle* ptr, const char** list, size_t size);

    // Returns how many bytes the current process has used.
    SVR_API size_t os_get_proc_mem_usage();

    // Returns whether or not a particular allocation would overflow the allowed working set.
    SVR_API bool os_would_alloc_overflow();

    struct os_file_list;

    // Lists files in a directory.
    // The returned file list must be destroyed.
    SVR_API os_file_list* os_list_files(const char* path);

    // Destroys a file system listing.
    SVR_API void os_destroy_file_list(os_file_list* ptr);

    // Returns how many files there are in a file listing.
    SVR_API size_t os_file_list_size(os_file_list* ptr);

    // Returns the full path to a file in a file listing.
    SVR_API const char* os_file_list_path(os_file_list* ptr, size_t index);

    // Returns the filename of a file in a file listing.
    SVR_API const char* os_file_list_name(os_file_list* ptr, size_t index);

    // Returns the extension of a file in a file listing.
    SVR_API const char* os_file_list_ext(os_file_list* ptr, size_t index);

    // Returns the current working directory.
    // Ends with a backslash.
    SVR_API bool os_get_current_dir(char* buf, size_t size);

    // Translates backslashes to frontslashes.
    SVR_API void os_normalize_path(char* buf, size_t size);

    // Returns whether or not a file exists on the filesystem.
    SVR_API bool os_does_file_exist(const char* path);

    // Structure which keeps track of sequential writing
    // to keep memory writings in order.
    class os_remote_proc_mem_writer
    {
    public:
        os_remote_proc_mem_writer(os_handle* proc_ptr, void* target_address)
        {
            proc = proc_ptr;
            address = target_address;
        }

        // Writes a region of memory to the target process.
        // Increases the internal position by the region size
        // so the next write will be placed after.
        void* write_region(const void* source, size_t size)
        {
            auto target_pos = (uint8_t*)address + pos;

            size_t written;

            if (!os_write_remote_proc_mem(proc, target_pos, source, size, &written))
            {
                return nullptr;
            }

            pos += written;

            return target_pos;
        }

        bool read_region(const void* source, void* dest, size_t size)
        {
            return os_read_remote_proc_mem(proc, source, dest, size, nullptr);
        }

        // Writes a null terminated string.
        // Includes the null terminator.
        const char* write_string(const char* value)
        {
            auto length = strlen(value);
            return static_cast<const char*>(write_region(value, length + 1));
        }

    private:
        os_handle* proc;
        void* address;

        ptrdiff_t pos = 0;
    };
}
