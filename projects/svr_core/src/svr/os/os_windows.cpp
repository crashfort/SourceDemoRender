#include <svr/os.hpp>
#include <svr/mem.hpp>
#include <svr/log_format.hpp>
#include <svr/defer.hpp>

#include <Windows.h>
#include <Psapi.h>

#include <string.h>

namespace svr
{
    os_handle* os_create_event(const char* name)
    {
        auto ret = (os_handle*)CreateEventA(nullptr, true, false, name);

        if (ret == nullptr)
        {
            log("windows: Could not create event '{}' ({})\n", name, GetLastError());
        }

        return ret;
    }

    os_handle* os_open_event(const char* name)
    {
        auto ret = (os_handle*)OpenEventA(EVENT_MODIFY_STATE, false, name);

        if (ret == nullptr)
        {
            log("windows: Could not open event '{}' ({})\n", name, GetLastError());
        }

        return ret;
    }

    void os_set_event(os_handle* ptr)
    {
        auto res = SetEvent((HANDLE)ptr);

        if (res == 0)
        {
            log("windows: Could not set event ({})\n", GetLastError());
        }
    }

    void os_reset_event(os_handle* ptr)
    {
        auto res = ResetEvent((HANDLE)ptr);

        if (res == 0)
        {
            log("windows: Could not reset event ({})\n", GetLastError());
        }
    }

    os_handle* os_handle_wait_any(os_handle** handles, size_t size, uint32_t timeout)
    {
        auto res = WaitForMultipleObjects(size, (HANDLE*)handles, false, timeout);

        if (res == WAIT_FAILED)
        {
            log("windows: Could not wait for any waitable ({})\n", GetLastError());
            return nullptr;
        }

        auto max_wait = WAIT_OBJECT_0 + size;

        if (res < max_wait)
        {
            auto index = res - WAIT_OBJECT_0;
            return *(handles + index);
        }

        if (res == STATUS_TIMEOUT)
        {
            return nullptr;
        }

        return nullptr;
    }

    void os_close_handle(os_handle* ptr)
    {
        auto res = CloseHandle((HANDLE)ptr);

        if (res == 0)
        {
            log("windows: Could not close handle ({})\n", GetLastError());
        }
    }

    void os_handle_wait_all(os_handle** handles, size_t size, uint32_t timeout)
    {
        auto res = WaitForMultipleObjects(size, (HANDLE*)handles, true, timeout);

        if (res == WAIT_FAILED)
        {
            log("windows: Could not wait for all waitables ({})\n", GetLastError());
            return;
        }
    }

    os_handle* os_open_mutex(const char* name)
    {
        auto ret = OpenMutexA(MUTEX_ALL_ACCESS, false, name);

        if (ret == nullptr)
        {
            log("windows: Could not open mutex '{}' ({})\n", name, GetLastError());
        }

        return (os_handle*)ret;
    }

    void os_release_mutex(os_handle* ptr)
    {
        auto res = ReleaseMutex((HANDLE)ptr);

        if (!res)
        {
            log("windows: Could not release mutex {} ({})\n", (void*)ptr, GetLastError());
        }
    }

    bool os_read_file(const char* path, mem_buffer& buffer)
    {
        auto file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            log("windows: Could not open file '{}' ({})\n", path, GetLastError());
            return false;
        }

        defer {
            CloseHandle(file);
        };

        LARGE_INTEGER large;

        if (GetFileSizeEx(file, &large) == 0)
        {
            log("windows: Could not retrieve size of file '{}' ({})\n", path, GetLastError());
            return false;
        }

        auto file_size = large.QuadPart;

        auto buf = &buffer;

        if (mem_create_buffer(*buf, file_size) == false)
        {
            return false;
        }

        defer {
            if (buf) mem_destroy_buffer(*buf);
        };

        DWORD read = 0;
        auto res = ReadFile(file, buf->data, file_size, &read, nullptr);

        if (res == 0)
        {
            log("windows: Could not read from file '{}'. Got {} bytes expected {} bytes ({})\n", path, read, file_size, GetLastError());
            return false;
        }

        buf = nullptr;
        return true;
    }

    bool os_write_file(const char* path, const void* data, size_t size)
    {
        auto file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file == INVALID_HANDLE_VALUE)
        {
            log("windows: Could not open file '{}' ({})\n", path, GetLastError());
            return false;
        }

        defer {
            CloseHandle(file);
        };

        DWORD written;
        auto res = WriteFile(file, data, size, &written, nullptr);

        if (res == 0)
        {
            log("windows: Could not write {} bytes to '{}'\n ({})\n", size, path);
            return false;
        }

        return true;
    }

    os_handle* os_create_pipe_read(const char* name)
    {
        auto BUFFER_SIZE = 1024 * 1024;

        fmt::memory_buffer buf;

        // Pipe names must be named like this.
        format_with_null(buf, R"(\\.\pipe\{})", name);

        auto pipe = CreateNamedPipeA(buf.data(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 1, BUFFER_SIZE, BUFFER_SIZE, 0, nullptr);

        if (pipe == INVALID_HANDLE_VALUE)
        {
            log("windows: Could not create pipe '{}' ({})\n", buf.data(), GetLastError());
            return nullptr;
        }

        return (os_handle*)pipe;
    }

    os_handle* os_open_pipe_write(const char* name)
    {
        fmt::memory_buffer buf;

        // Pipe names must be named like this.
        format_with_null(buf, R"(\\.\pipe\{})", name);

        auto pipe = CreateFileA(buf.data(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (pipe == INVALID_HANDLE_VALUE)
        {
            log("windows: Could not open pipe '{}' ({})\n", buf.data(), GetLastError());
            return nullptr;
        }

        return (os_handle*)pipe;
    }

    bool os_create_pipe_pair(os_handle** read, os_handle** write)
    {
        HANDLE read_h;
        HANDLE write_h;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = nullptr;
        sa.bInheritHandle = true;

        auto res = CreatePipe(&read_h, &write_h, &sa, 1024 * 1024 * 64);

        if (res == 0)
        {
            log("windows: Could not create anonymous pipe ({})\n", GetLastError());
            return false;
        }

        // Since we start all child processes with inherited handles, it would try to inherit the writing endpoint of this pipe too.
        // We have to remove the inheritance of this pipe. Otherwise processes that use these handles will never be able to stop.
        res = SetHandleInformation(write_h, HANDLE_FLAG_INHERIT, 0);

        *read = (os_handle*)read_h;
        *write = (os_handle*)write_h;

        return true;
    }

    bool os_read_pipe(os_handle* ptr, void* data, size_t size, size_t* read)
    {
        DWORD dummy;
        auto res = ReadFile(ptr, data, size, &dummy, nullptr);

        if (res == 0)
        {
            auto error = GetLastError();

            // If the writing end has closed their pipe, we are done.
            if (error != ERROR_BROKEN_PIPE)
            {
                log("windows: Could not read from pipe {} ({})\n", (void*)ptr, error);
            }

            return false;
        }

        if (read)
        {
            *read = dummy;
        }

        return true;
    }

    bool os_write_pipe(os_handle* ptr, const void* data, size_t size)
    {
        DWORD written;
        auto res = WriteFile(ptr, data, size, &written, nullptr);

        if (res == 0)
        {
            log("windows: Could not write {} bytes to pipe {} ({})\n", size, (void*)ptr, GetLastError());
            return false;
        }

        return true;
    }

    bool os_get_module_info(const char* name, os_module_info* ptr)
    {
        MODULEINFO info;

        auto mod = os_get_module(name);

        if (mod == nullptr)
        {
            return false;
        }

        auto res = K32GetModuleInformation(GetCurrentProcess(), (HMODULE)mod, &info, sizeof(info));

        if (res == 0)
        {
            log("windows: Could not get module information for '{}' ({})\n", name, GetLastError());
            return false;
        }

        ptr->base = info.lpBaseOfDll;
        ptr->size = info.SizeOfImage;
        return true;
    }

    os_module* os_get_module(const char* name)
    {
        auto ret = GetModuleHandleA(name);

        if (ret == nullptr)
        {
            log("windows: Could not get module with name '{}' ({})\n", name, GetLastError());
        }

        return (os_module*)ret;
    }

    void* os_get_module_function(os_module* mod, const char* name)
    {
        auto ret = GetProcAddress((HMODULE)mod, name);

        if (ret == nullptr)
        {
            char buf[MAX_PATH] = "N/A";
            os_get_module_name(os_get_proc_handle_self(), mod, buf, sizeof(buf));

            log("windows: Could not get export procedure '{}' in module '{}' ({})\n", name, buf, GetLastError());
        }

        return ret;
    }

    bool os_get_module_name(os_handle* proc, os_module* mod, char* buf, size_t size)
    {
        char temp[MAX_PATH];
        auto res = K32GetModuleFileNameExA(proc, (HMODULE)mod, temp, sizeof(temp));

        if (res == 0)
        {
            log("windows: Could not get module name ({})\n", GetLastError());
            return false;
        }

        auto written = res;

        auto view = temp;
        size_t view_len = 0;

        // If this is an absolute path, only keep the file name.

        for (DWORD i = written; i >= 0; i--)
        {
            if (temp[i] == '\\')
            {
                view += i + 1;
                view_len = written - i;
                break;
            }
        }

        if (size < view_len)
        {
            return false;
        }

        memcpy(buf, view, std::min(view_len, size));

        return true;
    }

    bool os_start_proc(const char* exe, const char* dir, const char* args, const os_start_proc_desc* desc, os_handle** proc_handle, os_handle** thread_handle)
    {
        // CreateProcess needs a writable buffer for some reason.
        auto args_copy = std::string(args);

        STARTUPINFOA start_info = {};
        start_info.cb = sizeof(start_info);

        DWORD flags = 0;

        if (desc)
        {
            start_info.hStdInput = desc->input_pipe;
            start_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
            start_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);

            if (desc->input_pipe)
            {
                start_info.dwFlags |= STARTF_USESTDHANDLES;
            }

            if (desc->suspended)
            {
                flags |= CREATE_SUSPENDED;
            }

            if (desc->hide_window)
            {
                flags |= CREATE_NO_WINDOW;
            }
        }

        PROCESS_INFORMATION info;

        auto res = CreateProcessA(exe, args_copy.data(), nullptr, nullptr, true, flags, nullptr, dir, &start_info, &info);

        if (res == 0)
        {
            log("windows: Could not create process '{}' in dir '{}' with args '{}' ({})\n", exe, dir, args, GetLastError());
            return false;
        }

        if (proc_handle)
        {
            *proc_handle = (os_handle*)info.hProcess;
        }

        else
        {
            os_close_handle((os_handle*)info.hProcess);
        }

        if (thread_handle)
        {
            *thread_handle = (os_handle*)info.hThread;
        }

        else
        {
            os_close_handle((os_handle*)info.hThread);
        }

        return true;
    }

    bool os_open_proc(uint64_t id, os_handle** proc_handle)
    {
        auto res = OpenProcess(PROCESS_ALL_ACCESS, false, id);

        if (res == nullptr)
        {
            log("windows: Could not open process {} ({})\n", id, GetLastError());
            return false;
        }

        *proc_handle = (os_handle*)res;

        return true;
    }

    uint64_t os_get_proc_id(os_handle* ptr)
    {
        return GetProcessId(ptr);
    }

    uint64_t os_get_proc_id_self()
    {
        return GetCurrentProcessId();
    }

    uint64_t os_get_thread_id(os_handle* ptr)
    {
        return GetThreadId(ptr);
    }

    uint64_t os_get_thread_id_self()
    {
        return GetCurrentThreadId();
    }

    os_handle* os_get_proc_handle_self()
    {
        return (os_handle*)GetCurrentProcess();
    }

    uint64_t os_get_proc_id_from_thread(os_handle* ptr)
    {
        return GetProcessIdOfThread(ptr);
    }

    void os_resume_thread(os_handle* ptr)
    {
        auto res = ResumeThread(ptr);

        if (res == (DWORD)-1)
        {
            auto thread_id = os_get_thread_id(ptr);
            auto proc_id = os_get_proc_id_from_thread(ptr);

            log("windows: Could not resume thread {} in process {} ({})\n", thread_id, proc_id, GetLastError());
        }
    }

    void os_terminate_proc(os_handle* ptr)
    {
        auto res = TerminateProcess(ptr, 1);

        if (res == 0)
        {
            auto proc_id = os_get_proc_id(ptr);
            log("windows: Could not terminate process {} ({})\n", proc_id, GetLastError());
        }
    }

    void os_terminate_proc_self()
    {
        auto res = TerminateProcess(GetCurrentProcess(), 1);

        if (res == 0)
        {
            auto proc_id = os_get_proc_id_self();
            log("windows: Could not terminate process {} ({})\n", proc_id, GetLastError());
        }
    }

    void* os_alloc_remote_proc_mem(os_handle* ptr, size_t size)
    {
        auto ret = VirtualAllocEx(ptr, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

        if (ret == nullptr)
        {
            auto proc_id = os_get_proc_id(ptr);
            log("windows: Could not remotely allocate {} bytes in process {} ({})\n", size, proc_id, GetLastError());
        }

        return ret;
    }

    void os_free_remote_proc_mem(os_handle* ptr, void* target)
    {
        auto res = VirtualFreeEx(ptr, target, 0, MEM_RELEASE);

        if (res == 0)
        {
            auto proc_id = os_get_proc_id(ptr);
            log("windows: Could not remotely free memory in process {} at location {} ({})\n", proc_id, target, GetLastError());
        }
    }

    bool os_write_remote_proc_mem(os_handle* ptr, void* target, const void* source, size_t size, size_t* written)
    {
        SIZE_T dummy;
        auto res = WriteProcessMemory(ptr, target, source, size, &dummy);

        if (res == 0)
        {
            auto proc_id = os_get_proc_id(ptr);
            log("windows: Could not write process {} memory at target {} with size {} ({})\n", proc_id, target, size, GetLastError());

            return false;
        }

        if (written)
        {
            *written = dummy;
        }

        return true;
    }

    bool os_read_remote_proc_mem(os_handle* ptr, const void* source, void* dest, size_t size, size_t* read)
    {
        SIZE_T dummy;
        auto res = ReadProcessMemory(ptr, source, dest, size, &dummy);

        if (res == 0)
        {
            auto proc_id = os_get_proc_id(ptr);
            log("windows: Could not read process {} memory at source {} with size {} ({})\n", proc_id, source, size, GetLastError());
            return false;
        }

        if (read)
        {
            *read = dummy;
        }

        return true;
    }

    bool os_queue_async_thread_func(os_handle* thread, void* func, void* param)
    {
        auto res = QueueUserAPC((PAPCFUNC)func, thread, (ULONG_PTR)param);

        if (res == 0)
        {
            auto proc_id = os_get_proc_id(thread);
            log("windows: Could not queue function {} in process {} ({})\n", func, proc_id, GetLastError());

            return false;
        }

        return true;
    }

    bool os_query_proc_modules(os_handle* ptr, os_module** list, size_t size, size_t* written)
    {
        // This api function requires the input to be in bytes, it also returns how many bytes are written.
        // If the size of HWMODULE and os_module* are equal then its guaranteed that the bytes written
        // will match.

        static_assert(sizeof(HMODULE) == sizeof(os_module*), "size mismatch");

        DWORD required_bytes;
        auto res = K32EnumProcessModules(ptr, (HMODULE*)list, size * sizeof(HMODULE), &required_bytes);

        if (res == 0)
        {
            log("windows: Could not query process modules ({})\n", GetLastError());
            return false;
        }

        if (written)
        {
            *written = required_bytes / sizeof(HMODULE);
        }

        return true;
    }

    size_t os_check_proc_modules(os_handle* ptr, const char** list, size_t size)
    {
        // Use a large array here.
        // Some games like CSGO use a lot of libraries (more than 128).

        os_module* module_list[384];
        size_t module_list_size;

        if (!os_query_proc_modules(ptr, module_list, 384, &module_list_size))
        {
            return 0;
        }

        size_t hits = 0;

        // See if any of the requested modules are loaded.

        for (size_t i = 0; i < module_list_size; i++)
        {
            char name[MAX_PATH];

            if (!os_get_module_name(ptr, module_list[i], name, sizeof(name)))
            {
                continue;
            }

            for (size_t i = 0; i < size; i++)
            {
                if (strcmp(list[i], name) == 0)
                {
                    hits++;
                    break;
                }
            }

            if (hits == size)
            {
                break;
            }
        }

        return hits;
    }

    size_t os_get_proc_mem_usage()
    {
        PROCESS_MEMORY_COUNTERS desc = {};

        auto res = K32GetProcessMemoryInfo(GetCurrentProcess(), &desc, sizeof(desc));

        if (res == 0)
        {
            log("windows: Could not retrieve process memory info ({})\n", GetLastError());
            return 0;
        }

        return desc.WorkingSetSize;
    }

    bool os_would_alloc_overflow()
    {
        // Soft limit for 32 bit applications.
        return os_get_proc_mem_usage() > INT32_MAX;
    }

    bool os_get_current_dir(char* buf, size_t size)
    {
        auto res = GetCurrentDirectoryA(size, buf);

        if (res == 0)
        {
            log("Could not get the current directory ({})\n", GetLastError());
            return false;
        }

        auto written = res;

        // Require that the path ends on a slash.
        // Ensure that there is room for it.

        if (size < written + 1)
        {
            return false;
        }

        buf[written] = '/';
        buf[written + 1] = 0;

        // Require to use forward slashes everywhere.
        os_normalize_path(buf, written);

        return true;
    }

    bool os_does_file_exist(const char* path)
    {
        auto attr = GetFileAttributesA(path);

        if (attr == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        return true;
    }
}
