#include <svr/game_config.hpp>
#include <svr/log_format.hpp>
#include <svr/table.hpp>
#include <svr/config.hpp>
#include <svr/str.hpp>

#include <svr/os.hpp>

#include <vector>

namespace svr
{
    // Epic encapsulation to hide the use of std vector.
    struct game_config_game
    {
        const char* id;
        const char* mutex;
        const char* arch;

        std::vector<const char*> libraries;
        std::vector<svr::game_config_comp*> comps;
    };

    struct game_config
    {
        std::vector<game_config_comp> comps;
        std::vector<game_config_game> games;
    };
}

static svr::game_config_comp_type map_comp_type(const char* value)
{
    using namespace svr;

    table types = {
        table_pair{"pattern", GAME_COMP_PATTERN},
        table_pair{"create-interface", GAME_COMP_CREATE_INTERFACE},
        table_pair{"virtual", GAME_COMP_VIRTUAL},
        table_pair{"export", GAME_COMP_EXPORT},
        table_pair{"offset", GAME_COMP_OFFSET},
    };

    return table_map_key_or(types, value, GAME_COMP_NONE);
}

// Attempts to set up a component from a configuration node.
static bool setup_comp_value(svr::game_config_comp* ptr, svr::config_node* n)
{
    using namespace svr;

    auto value = config_find(n, "value");

    if (value == nullptr)
    {
        log("Component '{}' missing a value node\n", ptr->config_id);
        return false;
    }

    if (ptr->code_type == GAME_COMP_PATTERN)
    {
        auto& v = ptr->pattern_value;
        v.library = config_view_string_or(config_find(value, "library"), nullptr);
        v.pattern = config_view_string_or(config_find(value, "pattern"), nullptr);
        v.offset = config_view_int64_or(config_find(value, "offset"), 0);
        v.relative_jump = config_view_bool_or(config_find(value, "relative-jump"), false);

        // Offset and relative are optional fields.

        if (v.library == nullptr)
        {
            log("Component '{}' missing a library node\n", ptr->config_id);
            return false;
        }

        if (v.pattern == nullptr)
        {
            log("Component '{}' missing a pattern node\n", ptr->config_id);
            return false;
        }
    }

    else if (ptr->code_type == GAME_COMP_CREATE_INTERFACE)
    {
        auto& v = ptr->create_interface_value;
        v.library = config_view_string_or(config_find(value, "library"), nullptr);
        v.interface_name = config_view_string_or(config_find(value, "interface-name"), nullptr);

        if (v.library == nullptr)
        {
            log("Component '{}' missing a library node\n", ptr->config_id);
            return false;
        }

        if (v.interface_name == nullptr)
        {
            log("Component '{}' missing an interface-name node\n", ptr->config_id);
            return false;
        }
    }

    else if (ptr->code_type == GAME_COMP_VIRTUAL)
    {
        auto& v = ptr->virtual_value;
        v.vtable_index = config_view_int64_or(config_find(value, "vtable-index"), -1);

        if (v.vtable_index == -1)
        {
            log("Component '{}' missing a vtable-index node\n", ptr->config_id);
            return false;
        }
    }

    else if (ptr->code_type == GAME_COMP_EXPORT)
    {
        auto& v = ptr->export_value;
        v.library = config_view_string_or(config_find(value, "library"), nullptr);
        v.export_name = config_view_string_or(config_find(value, "export-name"), nullptr);

        if (v.library == nullptr)
        {
            log("Component '{}' missing a library node\n", ptr->config_id);
            return false;
        }

        if (v.export_name == nullptr)
        {
            log("Component '{}' missing an export-name node\n", ptr->config_id);
            return false;
        }
    }

    else if (ptr->code_type == GAME_COMP_OFFSET)
    {
        auto& v = ptr->offset_value;
        v.value = config_view_int64_or(config_find(value, "offset"), INT64_MAX);

        if (v.value == INT64_MAX)
        {
            log("Component '{}' missing an offset node\n", ptr->config_id);
            return false;
        }
    }

    return true;
}

static svr::game_config_comp* find_comp(svr::game_config* cfg, const char* id)
{
    using namespace svr;

    for (auto& comp : cfg->comps)
    {
        if (strcmp(comp.config_id, id) == 0)
        {
            return &comp;
        }
    }

    return nullptr;
}

static bool parse_config(svr::game_config* cfg, svr::config_node* node)
{
    using namespace svr;

    auto comps_node = config_find(node, "components");
    auto games_node = config_find(node, "games");

    if (comps_node == nullptr)
    {
        log("Game config missing a components node\n");
        return false;
    }

    if (games_node == nullptr)
    {
        log("Game config missing a games node\n");
        return false;
    }

    cfg->comps.reserve(config_get_array_size(comps_node));

    for (auto e : config_node_iterator(comps_node))
    {
        game_config_comp new_comp;
        new_comp.code_type = map_comp_type(config_view_string_or(config_find(e, "code-type"), nullptr));
        new_comp.code_id = config_view_string_or(config_find(e, "code-id"), nullptr);
        new_comp.config_id = config_view_string_or(config_find(e, "config-id"), nullptr);
        new_comp.signature = config_view_string_or(config_find(e, "signature"), nullptr);

        if (new_comp.code_type == GAME_COMP_NONE)
        {
            log("A component has an unknown type or missing a code-type node\n");
            return false;
        }

        if (new_comp.code_id == nullptr)
        {
            log("A component is missing a code-id node\n");
            return false;
        }

        if (new_comp.config_id == nullptr)
        {
            log("A component is missing a config-id node\n");
            return false;
        }

        if (new_comp.signature == nullptr)
        {
            log("Component '{}' missing a signature node\n", new_comp.config_id);
            return false;
        }

        if (!setup_comp_value(&new_comp, e))
        {
            log("Could not setup component '{}'\n", new_comp.config_id);
            return false;
        }

        cfg->comps.push_back(new_comp);
    }

    cfg->games.reserve(config_get_array_size(games_node));

    for (auto game_entry : config_node_iterator(games_node))
    {
        game_config_game new_game;
        new_game.id = config_view_string_or(config_find(game_entry, "id"), nullptr);
        new_game.mutex = config_view_string_or(config_find(game_entry, "mutex"), nullptr);
        new_game.arch = config_view_string_or(config_find(game_entry, "arch"), nullptr);

        if (new_game.id == nullptr)
        {
            log("A game is missing an id node\n");
            return false;
        }

        if (new_game.arch == nullptr)
        {
            log("Game '{}' is missing an arch node\n", new_game.id);
            return false;
        }

        auto required_libraries = config_find(game_entry, "required-libraries");
        auto components = config_find(game_entry, "components");

        if (required_libraries == nullptr)
        {
            log("Game '{}' is missing required-libraries node\n", new_game.id);
            return false;
        }

        if (components == nullptr)
        {
            log("Game '{}' is missing a components node\n", new_game.id);
            return false;
        }

        new_game.libraries.reserve(config_get_array_size(components));

        for (auto e : config_node_iterator(required_libraries))
        {
            auto lib = config_view_string_or(e, nullptr);

            if (lib != nullptr)
            {
                new_game.libraries.push_back(lib);
            }

            else
            {
                log("Game '{}' has syntax errors in its library list. There must only be strings in this array\n", new_game.id);
            }
        }

        new_game.comps.reserve(config_get_array_size(components));

        for (auto e : config_node_iterator(components))
        {
            auto name = config_view_string_or(e, nullptr);

            if (name != nullptr)
            {
                auto comp = find_comp(cfg, name);

                if (comp == nullptr)
                {
                    log("Could not find script component '{}'\n", name);
                    return false;
                }

                new_game.comps.push_back(comp);
            }

            else
            {
                log("Game '{}' has syntax errors in its component list. There must only be strings in this array\n", new_game.id);
            }
        }

        cfg->games.push_back(new_game);
    }

    return true;
}

namespace svr
{
    game_config* game_config_parse(config_node* node)
    {
        auto cfg = new game_config;

        if (!parse_config(cfg, node))
        {
            log("Could not parse game config\n");
            game_config_destroy(cfg);
            return false;
        }

        return cfg;
    }

    void game_config_destroy(game_config* ptr)
    {
        delete ptr;
    }

    game_config_game* game_config_find_game(game_config* ptr, const char* id)
    {
        for (auto& entry : ptr->games)
        {
            if (strcmp(entry.id, id) == 0)
            {
                return &entry;
            }
        }

        return nullptr;
    }

    const char* game_config_game_id(game_config_game* ptr)
    {
        return ptr->id;
    }

    const char* game_config_game_arch(game_config_game* ptr)
    {
        return ptr->arch;
    }

    const char* game_config_game_mutex(game_config_game* ptr)
    {
        return ptr->mutex;
    }

    const char** game_config_game_libs(game_config_game* ptr)
    {
        return ptr->libraries.data();
    }

    size_t game_config_game_libs_size(game_config_game* ptr)
    {
        return ptr->libraries.size();
    }

    game_config_comp* game_config_game_find_comp(game_config_game* ptr, const char* type)
    {
        for (auto view : ptr->comps)
        {
            if (strcmp(view->code_id, type) == 0)
            {
                return view;
            }
        }

        return nullptr;
    }
}
