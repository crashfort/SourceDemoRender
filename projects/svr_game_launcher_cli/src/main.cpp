#include <svr/game.hpp>
#include <svr/config.hpp>
#include <svr/log_format.hpp>
#include <svr/os.hpp>
#include <svr/version.hpp>
#include <svr/defer.hpp>

#include <stdio.h>
#include <vector>

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
};

static void show_info()
{
    using namespace svr;

    log("SOURCE VIDEO RENDER\n");
    log("For more information see https://github.com/crashfort/SourceDemoRender\n");
}

static void show_version()
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

    auto version_data = version_parse(config_root(cfg));

    auto print_version = [](const char* caption, const version_pair& v)
    {
        log("{}: {}.{}\n", caption, v.major, v.minor);
    };

    print_version("Core version", version_data.core);
    print_version("Game config version", version_data.game_config);
    print_version("Game version", version_data.game);
    print_version("Launcher version", version_data.game_launcher_cli);
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

static void show_workarounds()
{
    svr::log("For the time being, fps_max 0 and mat_queue mode 0 must be inserted manually at appropriate locations. "
             "Unlimited framerate increases the performance a lot, and nonqueued material rendering ensures there is no flickering\n");
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

    return svr::game_launch_inject(game.exe_path, game.dir_path, game.id, game.args, buf);
}

static bool proc()
{
    using namespace svr;

    show_info();
    log("\n");
    show_version();
    log("\n");

    launcher_state state;

    if (!init(state))
    {
        return false;
    }

    if (!has_games(state))
    {
        log("No games to launch\n");
        return false;
    }

    show_workarounds();
    log("\n");

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

    // Return 0 on success.
    auto ret = !proc();

    log("You can close this window now\n");

    // Keep the console window open. In 99% cases the executable will be started directly from Explorer.
    // The window must remain open so all messages are shown.
    fgetc(stdin);

    return ret;
}
