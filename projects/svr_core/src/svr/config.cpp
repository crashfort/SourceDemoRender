#include <svr/config.hpp>
#include <svr/os.hpp>
#include <svr/mem.hpp>
#include <svr/log_format.hpp>
#include <svr/defer.hpp>

#include <nlohmann/json.hpp>

static nlohmann::json* convert_to_json(svr::config_node* ptr)
{
    return (nlohmann::json*)ptr;
}

// Embeds the contents of a json ptr into a config view.
static svr::config_node* convert_from_json(nlohmann::json* ptr)
{
    return (svr::config_node*)ptr;
}

static nlohmann::json* find_json(nlohmann::json* ptr, const char* name)
{
    for (auto&[key, value] : ptr->items())
    {
        if (key == name)
        {
            return &value;
        }
    }

    return nullptr;
}

namespace svr
{
    struct config
    {
        nlohmann::json json;
    };
}

namespace svr
{
    config* config_open_json(const char* path)
    {
        mem_buffer buffer;

        if (!os_read_file(path, buffer))
        {
            log("json: Could not open file '{}'\n", path);
            return nullptr;
        }

        defer {
            mem_destroy_buffer(buffer);
        };

        auto start = (const char*)buffer.data;
        auto size = buffer.size;

        // The json library will throw std::exception on parsing failure.

        try
        {
            auto json = nlohmann::json::parse(start, start + size);

            auto ret = new config;
            ret->json = std::move(json);

            return ret;
        }

        catch (const std::exception& error)
        {
            log("json: {}\n", error.what());
            log("json: Could not parse file '{}'\n", path);
        }

        return nullptr;
    }

    config* config_parse_json(const char* data, size_t size)
    {
        // The json library will throw std::exception on parsing failure.

        try
        {
            auto json = nlohmann::json::parse(data, data + size);

            auto ret = new config;
            ret->json = std::move(json);

            return ret;
        }

        catch (const std::exception& error)
        {
            log("json: {}\n", error.what());
        }

        return nullptr;
    }

    void config_destroy(config* ptr)
    {
        delete ptr;
    }

    config_node* config_root(config* ptr)
    {
        return convert_from_json(&ptr->json);
    }
}

namespace svr
{
    config_node* config_find(config_node* node, const char* name)
    {
        auto json = find_json(convert_to_json(node), name);
        return convert_from_json(json);
    }

    bool config_get_bool(config_node* node, bool* out)
    {
        auto json = convert_to_json(node);

        if (json && json->is_boolean())
        {
            *out = json->get<bool>();
            return true;
        }

        return false;
    }

    bool config_get_int64(config_node* node, int64_t* out)
    {
        auto json = convert_to_json(node);

        if (json && json->is_number_integer())
        {
            *out = json->get<int64_t>();
            return true;
        }

        return false;
    }

    bool config_get_float(config_node* node, float* out)
    {
        auto json = convert_to_json(node);

        if (json && json->is_number_float())
        {
            *out = json->get<float>();
            return true;
        }

        return false;
    }

    const char* config_get_string(config_node* node, size_t* length)
    {
        auto json = convert_to_json(node);

        if (json && json->is_string())
        {
            auto str = json->get_ptr<nlohmann::json::string_t*>();

            if (length)
            {
                *length = str->size();
            }

            return str->c_str();
        }

        return nullptr;
    }

    size_t config_get_array_size(config_node* node)
    {
        auto json = convert_to_json(node);

        if (json && json->is_array())
        {
            return json->size();
        }

        return 0;
    }

    config_node* config_get_array_element(config_node* node, size_t index)
    {
        auto json = convert_to_json(node);

        if (json && json->is_array())
        {
            return convert_from_json(&json->at(index));
        }

        return convert_from_json(nullptr);
    }
}
