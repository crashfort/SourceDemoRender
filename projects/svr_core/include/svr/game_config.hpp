#pragma once
#include <svr/api.hpp>

#include <stdint.h>

namespace svr
{
    struct config_node;

    enum game_config_comp_type
    {
        GAME_COMP_NONE,
        GAME_COMP_PATTERN,
        GAME_COMP_CREATE_INTERFACE,
        GAME_COMP_VIRTUAL,
        GAME_COMP_EXPORT,
        GAME_COMP_OFFSET,
    };

    struct game_config_comp_pattern
    {
        const char* library;
        const char* pattern;
        ptrdiff_t offset;
        bool relative_jump;
    };

    struct game_config_comp_create_interface
    {
        const char* library;
        const char* interface_name;
    };

    struct game_config_comp_virtual
    {
        int vtable_index;
    };

    struct game_config_comp_export
    {
        const char* library;
        const char* export_name;
    };

    struct game_config_comp_offset
    {
        ptrdiff_t value;
    };

    struct game_config_comp
    {
        game_config_comp_type code_type;
        const char* code_id;
        const char* config_id;
        const char* signature;

        union
        {
            game_config_comp_pattern pattern_value;
            game_config_comp_create_interface create_interface_value;
            game_config_comp_virtual virtual_value;
            game_config_comp_export export_value;
            game_config_comp_offset offset_value;
        };
    };

    struct game_config;
    struct game_config_game;

    SVR_API game_config* game_config_parse(config_node* node);
    SVR_API void game_config_destroy(game_config* ptr);

    SVR_API game_config_game* game_config_find_game(game_config* ptr, const char* id);

    SVR_API const char* game_config_game_id(game_config_game* ptr);
    SVR_API const char* game_config_game_arch(game_config_game* ptr);
    SVR_API const char** game_config_game_libs(game_config_game* ptr);
    SVR_API size_t game_config_game_libs_size(game_config_game* ptr);
    SVR_API game_config_comp* game_config_game_find_comp(game_config_game* ptr, const char* type);
}
