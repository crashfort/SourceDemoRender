#include <svr/game.hpp>
#include <svr/config.hpp>
#include <svr/log_format.hpp>
#include <svr/os.hpp>
#include <svr/version.hpp>
#include <svr/defer.hpp>
#include <svr/str.hpp>

#include <stdio.h>
#include <vector>

#include <restclient-cpp/restclient.h>

static const char* autostart_game_id;

struct launcher_game
{
    const char* id;
    const char* display_name;
    const char* exe_path;
    const char* dir_path;
    const char* args;
};

struct launcher_state
{
    svr::config* launcher_config;

    // Elements are views to data in launcher_config.
    std::vector<launcher_game> game_list;

    svr::version_data version;
};

static void show_info()
{
    using namespace svr;

    log("SOURCE VIDEO RENDER\n");
    log("For more information see https://github.com/crashfort/SourceDemoRender\n");
}

static void get_local_version(launcher_state& state)
{
    using namespace svr;

    auto cfg = config_open_json("data/version.json");

    if (cfg == nullptr)
    {
        log("Could not open version file\n");
        return;
    }

    defer {
        config_destroy(cfg);
    };

    state.version = version_parse(config_root(cfg));

    auto print_version = [](const char* caption, const version_pair& v)
    {
        log("{}: {}.{}\n", caption, v.major, v.minor);
    };

    print_version("Application version", state.version.app);
}

struct manual_updatable
{
    const char* name;
    svr::version_pair local;
    svr::version_pair remote;
};

struct auto_updatable
{
    const char* file;
    const char* url;
};

static void check_updates(launcher_state& state)
{
    using namespace svr;

    log("\n");

    if (os_does_file_exist("no_update"))
    {
        log("Skipping update\n");
        return;
    }

    const auto BASE_URL = "https://raw.githubusercontent.com/crashfort/SourceDemoRender/svr/";

    str_builder url_builder;

    auto build_url = [&](const char* part)
    {
        url_builder.reset();
        url_builder.append(BASE_URL);
        url_builder.append(part);

        return url_builder.buf;
    };

    log("Looking for updates\n");

    auto ver_req = RestClient::get(build_url("bin/data/version.json"));

    if (ver_req.code != 200)
    {
        log("Could not query remote application version\n");
        return;
    }

    auto cfg = config_parse_json(ver_req.body.data(), ver_req.body.size());

    if (cfg == nullptr)
    {
        log("Could not parse remote version json\n");
        return;
    }

    defer {
        config_destroy(cfg);
    };

    auto local_v = state.version;
    auto remote_v = version_parse(config_root(cfg));

    manual_updatable manuals[] = {
        manual_updatable{"svr", local_v.app, remote_v.app},
    };

    // The game config is kept separate to allow for hotfixing of broken games, which may not require any application changes.

    auto_updatable autos[] = {
        auto_updatable{"data/game-config.json", "bin/data/game-config.json"},
    };

    auto print_comparison = [](const char* name, version_pair old, version_pair newer)
    {
        log("* {} ({}.{} -> {}.{})\n", name, old.major, old.minor, newer.major, newer.minor);
    };

    auto has_app_updates = false;

    for (auto& e : manuals)
    {
        if (version_greater_than(e.remote, e.local))
        {
            has_app_updates = true;
            break;
        }
    }

    if (has_app_updates)
    {
        log("There are updates available. Please visit https://github.com/crashfort/SourceDemoRender/releases to update!\n");

        for (auto e : manuals)
        {
            print_comparison(e.name, e.local, e.remote);
        }

        log("\n");
    }

    log("The following files can be automatically updated:\n");

    for (auto e : autos)
    {
        log("* {}\n", e.file);
    }

    log("\n");

    for (auto e : autos)
    {
        auto url = build_url(e.url);

        log("Downloading from '{}': ", url);

        auto req = RestClient::get(url);

        log("{}\n", req.code);

        if (req.code == 200)
        {
            log("Writing {} bytes to '{}'\n", req.body.size(), e.file);
            os_write_file(e.file, req.body.data(), req.body.size());
        }

        else
        {
            log("Could not download from '{}'\n", url);
        }
    }
}

static bool init(launcher_state& state)
{
    using namespace svr;

    state.launcher_config = config_open_json("data/launcher-config.json");

    if (state.launcher_config == nullptr)
    {
        log("Could not open launcher config\n");
        return false;
    }

    auto root = config_root(state.launcher_config);

    auto games_node = config_find(root, "games");

    if (games_node == nullptr)
    {
        log("Launcher config missing games array\n");
        return false;
    }

    state.game_list.reserve(config_get_array_size(games_node));

    for (auto entry : config_node_iterator(games_node))
    {
        launcher_game game;
        game.id = config_view_string_or(config_find(entry, "id"), "");
        game.display_name = config_view_string_or(config_find(entry, "display-name"), "");
        game.exe_path = config_view_string_or(config_find(entry, "exe-path"), "");
        game.dir_path = config_view_string_or(config_find(entry, "dir-path"), "");
        game.args = config_view_string_or(config_find(entry, "args"), "");

        // Require that a game has every field filled in to be added.
        // It is meant for the user to fill all these in.

        if (*game.id == 0 ||
            *game.display_name == 0 ||
            *game.exe_path == 0 ||
            *game.dir_path == 0 ||
            *game.args == 0)
        {
            continue;
        }

        state.game_list.push_back(game);
    }

    return true;
}

static bool has_games(launcher_state& state)
{
    return state.game_list.size() > 0;
}

static void show_instructions()
{
    svr::log("Select which number to start: ");
}

static void list_games(launcher_state& state)
{
    using namespace svr;

    log("These games were found in launcher-config.json:\n");

    size_t index = 0;

    for (const auto& game : state.game_list)
    {
        log("[{}] {} ({})\n", index + 1, game.display_name, game.id);
        index++;
    }
}

static int get_selection()
{
    char buf[32];
    auto res = fgets(buf, sizeof(buf), stdin);

    if (res == nullptr)
    {
        return 0;
    }

    auto selection = atoi(buf);

    if (selection == 0)
    {
        return 0;
    }

    return selection;
}

static bool start_game(launcher_state& state, int index)
{
    char buf[512];

    if (!svr::os_get_current_dir(buf, sizeof(buf)))
    {
        svr::log("Could not get the current directory\n");
        return false;
    }

    const auto& game = state.game_list[index];

    svr::log("Starting '{}'\n", game.display_name);

    return svr::game_launch_inject(game.exe_path, game.dir_path, game.id, game.args, buf, nullptr);
}

static int find_game_by_id(launcher_state& state, const char* id)
{
    auto index = 0;

    for (auto game : state.game_list)
    {
        if (strcmp(game.id, id) == 0)
        {
            return index;
        }

        index++;
    }

    return -1;
}

static bool proc()
{
    using namespace svr;

    launcher_state state;

    show_info();
    log("\n");

    get_local_version(state);
    check_updates(state);
    log("\n");

    if (!init(state))
    {
        return false;
    }

    if (!has_games(state))
    {
        log("No games to launch\n");
        return false;
    }

    if (autostart_game_id)
    {
        log("Trying to autostart game '{}'\n", autostart_game_id);
        log("\n");

        auto index = find_game_by_id(state, autostart_game_id);

        if (index == -1)
        {
            log("Could not find any game with id '{}'\n", autostart_game_id);
            return false;
        }

        return start_game(state, index);
    }

    list_games(state);
    log("\n");

    show_instructions();

    auto selection = get_selection();

    if (selection == 0 || selection > state.game_list.size())
    {
        return false;
    }

    log("\n");

    return start_game(state, selection - 1);
}

int main(int argc, char* argv[])
{
    using namespace svr;

    log_set_function([](void* context, const char* text)
    {
        printf(text);
    }, nullptr);

    if (argc > 1)
    {
        autostart_game_id = argv[1];
    }

    // Return 0 on success.
    auto ret = !proc();

    log("You can close this window now\n");

    // Keep the console window open. In 99% cases the executable will be started directly from Explorer.
    // The window must remain open so all messages are shown.
    fgetc(stdin);

    return ret;
}
