#include <svr/os.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace svr
{
    struct os_file_list
    {
        struct entry
        {
            std::string path;
            std::string name;
            std::string ext;
        };

        std::vector<entry> results;
    };

    os_file_list* os_list_files(const char* path)
    {
        auto ret = new os_file_list;

        for (const auto& it : std::filesystem::directory_iterator(path))
        {
            const auto& path = it.path();

            // This is ridiculous, but using std filesystem is simpler than anything else.

            os_file_list::entry entry;
            entry.path = path.generic_u8string();
            entry.name = path.filename().replace_extension().u8string();
            entry.ext = path.filename().extension().u8string();

            ret->results.push_back(std::move(entry));
        }

        return ret;
    }

    void os_destroy_file_list(os_file_list* ptr)
    {
        delete ptr;
    }

    size_t os_file_list_size(os_file_list* ptr)
    {
        return ptr->results.size();
    }

    const char* os_file_list_path(os_file_list* ptr, size_t index)
    {
        const auto& e = ptr->results[index];
        return e.path.c_str();
    }

    const char* os_file_list_name(os_file_list* ptr, size_t index)
    {
        const auto& e = ptr->results[index];
        return e.name.c_str();
    }

    const char* os_file_list_ext(os_file_list* ptr, size_t index)
    {
        const auto& e = ptr->results[index];
        return e.ext.c_str();
    }

    void os_normalize_path(char* buf, size_t size)
    {
        for (size_t i = 0; i < size; i++)
        {
            if (*buf == '\\')
            {
                *buf = '/';
            }

            buf++;
        }
    }
}
