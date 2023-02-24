# Source Video Render
[Download](https://github.com/crashfort/SourceDemoRender/releases)

[Discord](https://discord.gg/5t8D68c)

[Video editor for SVR content](https://movisum.com/) ([Discord](https://discord.gg/U5c9FREeuw))

Source Video Render (SVR, formely SDR) can be used to record movies for the Source engine with way higher performance than the built in `startmovie` command. SVR does not have video effects - if you need video effects, see [HLAE](https://www.advancedfx.org/).

SVR can record faster than realtime for normal videos (a 3 minute video can be recorded in 42 seconds). Videos can also be created with high quality motion blur (this is a slower process).

## Updates
You can use `update.cmd` in the SVR directory to automatically download the latest release. The latest SVR will be downloaded to `svr.zip`. You can extract this folder and SVR is now updated.

## Game support
| Game          | Windows
| ------------- | -----------------------
| Counter-Strike: Source           | ✔
| Counter-Strike: Global Offensive | ✔
| Team Fortress 2                  | ✔
| Zombie Panic! Source             | ✔
| Empires                          | ✔
| Half-Life 2                      | ✔
| Half-Life 2: Deathmatch          | ✔
| Source 2013 SP mods              | ✔
| Synergy                          | ✔
| Black Mesa                       | ✔
| Hunt Down The Freeman            | ✔

## Prerequisites
Any DirectX 11 (Direct3D 11.3) compatible graphics adapter with minimum of Windows 10 1909 is required. Hardware feature support verification will occur when starting the launcher.

## Startup
Use `svr_launcher.exe` or `svr_injector.exe` to start SVR. The launcher will scan the installed Steam games in your system. The supported games will be listed and can be started. The launch parameters will be read from Steam so Steam must be started. If you don't want to use the launch parameters from Steam, you can create a file called `svr_launch_params.ini` in the same folder as the launcher and insert a format like this (one line per game):

```ini
240=-width 2560 -height 1440
```

Left of the equal sign is Steam app id and everything to the right are the parameters to add.

It's possible to launch Source 2013 SP mods using this file by using the 220 app id (Half-Life 2) with a custom `-game` parameter. If a custom game parameter is used, the one specified by SVR will not be used. Do it like this:

```ini
220=-game <mod_name>
```

The following launch parameters are always used: ``-steam -insecure +sv_lan 1 -console -novid``.

When using `svr_launcher.exe` you are starting the standalone SVR, which modifies existing games to add SVR support. SVR stores the game build which it was tested and known to work on. In case a game updates, SVR may stop working and this will be printed to `SVR_LOG.TXT`.

When using `svr_injector.exe` you can start SVR in an already running game. The injector will only list the supported games that are running in insecure mode. **This is an unsupported method as there is no compatibility with interoperability between different applications. You may get unexpected results or crashes.** When injecting you will not be able to specify the launch parameters.

## Recording
Once in game, you can use the `startmovie` console command to start recording a movie and `endmovie` to stop. The `startmovie` command takes 1 or 2 parameters in this format: `startmovie <name> (<profile>)`. The *name* is the filename of the movie which will be located in `data/`. **If the name does not contain an extension (container), mp4 will automatically be selected.**. The *profile* is an optional parameter that decides which settings this movie will use. If not specified, the default profile is used (see Profiles below about profiles).

It's possible to start recording in the main menu but content will only be saved when outside of the main menu. Recording automatically stops when returning back to the main menu, unless ``-svrnoautostop`` is passed in as a launch parameter to the game. If autostop is disabled, SVR will not keep recording the menu, but will start recording again when connected or playing a demo.

When starting and ending a movie, the files `data/cfg/svr_movie_start_user.cfg` and `data/cfg/svr_movie_end_user.cfg` in `data/cfg` will be executed (create these if you want to have them). This can be used to insert commands that should be active only during the movie period. Note that these files are **not** in the game directory, but in the SVR directory. You can have game specific cfgs by using files called `dat/cfg/svr_movie_start_<app_id>.cfg` and `data/cfg/svr_movie_end_<app_id>.cfg`. The `app_id` should be substituted for the Steam app id, such as 240 for Counter-Strike: Source.

In case you want to override SVR settings you can edit `data/cfg/svr_movie_start_user.cfg` or `data/cfg/svr_movie_end_user.cfg`. Create these files if you want to use them. It is recommended that you don't edit `svr_movie_start.cfg` and `svr_movie.end.cfg` as they may be changed in updates, which would overwrite your changes.

The execution order of the cfgs is as follows: `svr_movie_start.cfg` > `svr_movie_start_user.cfg` > `svr_movie_start_<app_id>.cfg`. Each cfg can override the previous.

## Something's not working
If something is not working properly, please find the `SVR_LOG.TXT` file in the `data/` directory of SVR and explain what you were doing and upload it to [Discord](https://discord.gg/5t8D68c).

## Interoperability with other programs
Due to the nature of reverse engineering games, it cannot be trusted that direct interoperability (with ReShade or HLAE for example) will work because at the risk of collision. You can use `svr_injector.exe` at your own risk, but this is unsupported.

## Profiles
All recording settings are loaded from profiles which are located in `data/profiles`. The default profile is called `default.ini` and is the profile that will be used in case none is specified when starting a movie. These profiles are shared across all games. The settings of a profile is described below. All profiles are written in a simple INI format.

The default profile is used if none is specified when starting the movie. You can create your own profiles by copying `default.ini` and renaming it and making your changes. When starting your movie you can then specify your new profile. See Recording above.

Note that if you edit the default profile, you will lose your changes when updating SVR.

The documentation for profiles are written in `default.ini`.

## Motion blur demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher samples per second will remove banding at cost of slower recording times. For slower scenes you may get away with a lower sampling rate. Exposure is dependant on the type of content being made. The goal you should be aiming for is to reduce the banding that happens with lower samples per second. A smaller exposure will leave shorter trails of motion blur.

The X axis is the samples per second and the Y axis is the exposure (click on the images to see them larger).
|      | 960                           | 1920                           | 3840                           | 7680
| ---- | ----------------------------- | ------------------------------ | ------------------------------ | ------------------------------
| 0.25 | ![mosample_960_025](https://user-images.githubusercontent.com/3614412/134065919-991ff82e-ef79-45d7-8fd6-477f4d268580.png) | ![mosample_1920_025](https://user-images.githubusercontent.com/3614412/134065963-dc0acd84-ed73-4beb-8c55-7fa8d30973a0.png) | ![mosample_3840_025](https://user-images.githubusercontent.com/3614412/134065971-23085cbf-567c-409b-a426-a0352a2e921c.png) | ![mosample_7680_025](https://user-images.githubusercontent.com/3614412/134065978-e78c865a-f921-4743-9889-988700b0291d.png)
| 0.50 | ![mosample_960_050](https://user-images.githubusercontent.com/3614412/134065956-9b5a75d1-3c41-4dc0-b4fb-c9787c63bbeb.png) | ![mosample_1920_050](https://user-images.githubusercontent.com/3614412/134065965-bd1dba93-cd04-4c2c-880a-23907cb823a6.png) | ![mosample_3840_050](https://user-images.githubusercontent.com/3614412/134065972-24d4ce15-7528-4fdc-9ee2-509aa6cbc9fc.png) | ![mosample_7680_050](https://user-images.githubusercontent.com/3614412/134065979-2c158e36-03a2-46cb-bd5d-b461a9580eef.png)
| 0.75 | ![mosample_960_075](https://user-images.githubusercontent.com/3614412/134065958-f2f9a2ed-ac75-44e6-a23d-ff8bd845db74.png) | ![mosample_1920_075](https://user-images.githubusercontent.com/3614412/134065967-51956d12-c611-4365-85a3-d4b0841cd8b0.png) | ![mosample_3840_075](https://user-images.githubusercontent.com/3614412/134065975-290e4508-2b02-4336-8b67-310f9a8b6ef8.png) | ![mosample_7680_075](https://user-images.githubusercontent.com/3614412/134065981-4474b397-e073-465f-8e4e-776031c3994f.png)
| 1.00 | ![mosample_960_100](https://user-images.githubusercontent.com/3614412/134065959-919d64e8-29b4-4d08-96bc-6e9c323082c4.png) | ![mosample_1920_100](https://user-images.githubusercontent.com/3614412/134065969-bef4d03d-3cc2-490e-bdd2-ab17db41978f.png) | ![mosample_3840_100](https://user-images.githubusercontent.com/3614412/134065977-9ed70fba-a8af-4e67-92cc-ca02b5d7bf5a.png) | ![mosample_7680_100](https://user-images.githubusercontent.com/3614412/134065982-991ba3c1-5b1a-4aef-8f5b-b54abf68cc47.png)
