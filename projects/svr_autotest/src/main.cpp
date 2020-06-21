#include <svr/os.hpp>
#include <svr/config.hpp>
#include <svr/log.hpp>
#include <svr/game.hpp>

#include <stdio.h>

int main(int argc, char* argv[])
{
    using namespace svr;

    log_set_function([](void* context, const char* text)
    {
        printf(text);
    }, nullptr);

    auto cfg = config_open_json("data/launcher-config.json");

    if (cfg == nullptr)
    {
        log("Could not open launcher config\n");
        return 1;
    }

    auto root = config_root(cfg);

    auto games_node = config_find(root, "games");

    if (games_node == nullptr)
    {
        log("Launcher config missing games array\n");
        return 1;
    }

    char cur_dir[512];

    if (!svr::os_get_current_dir(cur_dir, sizeof(cur_dir)))
    {
        log("Could not get the current directory\n");
        return 1;
    }

    // Use the same conditions as the launcher.

    for (auto entry : config_node_iterator(games_node))
    {
        auto id = config_view_string_or(config_find(entry, "id"), "");
        auto display_name = config_view_string_or(config_find(entry, "display-name"), "");
        auto exe_path = config_view_string_or(config_find(entry, "exe-path"), "");
        auto dir_path = config_view_string_or(config_find(entry, "dir-path"), "");
        auto args = config_view_string_or(config_find(entry, "args"), "");

        // Require that a game has every field filled in to be added.
        // It is meant for the user to fill all these in.

        if (*id == 0 ||
            *display_name == 0 ||
            *exe_path == 0 ||
            *dir_path == 0 ||
            *args == 0)
        {
            continue;
        }

        os_handle* proc_handle;

        if (!game_launch_inject(exe_path, dir_path, id, args, cur_dir, &proc_handle))
        {
            return 1;
        }

        os_terminate_proc(proc_handle);
        os_handle_wait(proc_handle, -1);
        os_close_handle(proc_handle);
    }

    return 0;
}
