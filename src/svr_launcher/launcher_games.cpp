#include "launcher_priv.h"

LauncherGame SUPPORTED_GAMES[] = {
    LauncherGame { STEAM_GAME_CSS, "Counter-Strike: Source", "-game cstrike", "common\\Counter-Strike Source\\", "hl2.exe", 6946501 },
    LauncherGame { STEAM_GAME_CSGO, "Counter-Strike: Global Offensive", "-game csgo", "common\\Counter-Strike Global Offensive\\", "csgo.exe", 8128170 },
    LauncherGame { STEAM_GAME_TF2, "Team Fortress 2", "-game tf", "common\\Team Fortress 2\\", "hl2.exe", 7504322 },
    LauncherGame { STEAM_GAME_ZPS, "Zombie Panic! Source", "-game zps", "common\\Zombie Panic Source\\", "zps.exe", 5972042 },
    LauncherGame { STEAM_GAME_EMPIRES, "Empires", "-game empires", "common\\Empires\\", "hl2.exe", 8658619 },
    LauncherGame { STEAM_GAME_SYNERGY, "Synergy", "-game synergy", "common\\Synergy\\", "synergy.exe", 791804 },
    LauncherGame { STEAM_GAME_HL2, "Half-Life 2", "-game hl2", "common\\Half-Life 2\\", "hl2.exe", 4233294 },
    LauncherGame { STEAM_GAME_HL2DM, "Half-Life 2: Deathmatch", "-game hl2mp", "common\\Half-Life 2 Deathmatch\\", "hl2.exe", 6935373 },
    LauncherGame { STEAM_GAME_BMS, "Black Mesa", "-game bms", "common\\Black Mesa\\", "bms.exe", 4522431 },
    LauncherGame { STEAM_GAME_HDTF, "Hunt Down The Freeman", "-game hdtf", "common\\Hunt Down The Freeman\\", "hdtf.exe", 2604730 },
};

const s32 NUM_SUPPORTED_GAMES = SVR_ARRAY_SIZE(SUPPORTED_GAMES);
