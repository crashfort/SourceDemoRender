# Source Video Render
[Download](https://github.com/crashfort/SourceDemoRender/releases)

[Discord](https://discord.gg/5t8D68c)

Source Video Render (SVR, formely SDR) is used to record movies for the Source engine. SVR does not have video effects - if you need video effects, see [HLAE](https://www.advancedfx.org/).

SVR can record faster than realtime for normal videos, or with high quality motion blur.

## Updates
You can use `update.cmd` in the SVR directory to automatically download the latest release. The latest SVR will be downloaded to `svr.zip`. You can extract this folder and SVR is now updated.

## Prerequisites
Any DirectX 11 (Direct3D 11.3, feature level 12_0) compatible graphics adapter with minimum of Windows 10 1909 is required. Hardware feature support verification will occur when starting the launcher.

## Startup
Use `svr_launcher.exe` or `svr_launcher64.exe` to start the standalone SVR launcher. The launcher allows you to select which game you want to start. The games database is stored in the `data/games` directory. Here you can create new game profiles if you wish to change the launch parameters or configurations. The following launch parameters are always added by the launcher: ``-steam -insecure +sv_lan 1 -console -novid``.

The menu selection of the launcher can be bypassed by passing in the identification name for a game as a start argument to the launcher. As an example:

```
svr_launcher.exe cstrike_steam.ini
```

## Recording
Once in game, you can use the `startmovie` console command to start recording a movie and the `endmovie` command to stop.

The `startmovie` command should be used like this:

```
startmovie <file> (<optional parameters>)
```

As an example:

```
startmovie movie.mov timeout=30
```

The above example will produce a video called `movie.mov` with a render region of 30 seconds. Videos will be placed in the `data/movies/` directory.

The list of optional parameters are as follows:

| Parameter         | Description
| ----------------- | -----------
| ``timeout=<seconds>`` | Automatically stop rendering after the elapsed video time passes. This will add a progress bar to the task bar icon. By default, there is no timeout.
| ``profile=<string>`` | Override which rendering profile to use. If omitted, the default profile is used. See below about profiles.
| ``autostop=<value>`` | Automatically stop the movie on demo disconnect. This can be 0 or 1. Default is 1. This is used to determine what happens when a demo ends, when you get kicked back to the main menu.
| ``nowindupd=<value>`` | Disable window presentation. This can be 0 or 1. Default is 0. For some systems this may improve performance, however you will not be able to see anything.

When starting and ending a movie, the files `data/cfg/svr_movie_start_user.cfg` and `data/cfg/svr_movie_end_user.cfg` in `data/cfg` will be executed (you can create these if you want to use them). This can be used to insert or overwrite commands that should be active only during the movie period. Note that these files are **not** in the game directory, but in the SVR directory in `data/cfg`.

**It is recommended that you don't edit `svr_movie_start.cfg` and `svr_movie.end.cfg` as they may be changed in updates, which would overwrite your changes.**

The execution order of the cfgs is as follows: `svr_movie_start.cfg > svr_movie_start_user.cfg`. Each cfg file can override the previous.

The commands that are placed in `svr_movie_start.cfg` are required and must not be overwritten. Most notably, the variable `mat_queue_mode` must be 0 during recording for recording to work properly.

## Something's not working
If something is not working properly, please find the `SVR_LOG.txt` and `ENCODER_LOG.txt` file in the `data/` directory and explain what you were doing and upload it to [Discord](https://discord.gg/5t8D68c) or create a new issue here.

## Interoperability with other programs
Due to the nature of reverse engineering games, it cannot be trusted that direct interoperability (with ReShade or HLAE for example) will work because the risk of collision.

## Profiles
All recording settings are loaded from profiles which are located in `data/profiles`. The default profile is called `default.ini` and is the base profile of all settings. These profiles are shared across all games and are written in a simple INI format. The documentation for profiles is written inside the default profile [here](bin/data/profiles/default.ini).

The default profile is used if no other is specified when starting the movie. You can override settings in the default profile by creating your own profiles inside `data/profiles`. When starting your movie you can then specify your new profile (see Recording above).

The default profile is always loaded first, and your custom profile is loaded on top. This allows you to override individual setting without copying the entire profile. To create your own profile, create a file with an `.ini` extension inside `data/profiles/`. You can now override settings in the default profile by putting in the settings you want to override.

## Motion blur demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher samples per second will remove banding at cost of slower recording times. For slower scenes you may get away with a lower sampling rate. Exposure is dependant on the type of content being made. The goal you should be aiming for is to reduce the banding that happens with lower samples per second. A smaller exposure will leave shorter trails of motion blur.

The X axis is the samples per second and the Y axis is the exposure (click on the images to see them larger).
|      | 960                           | 1920                           | 3840                           | 7680
| ---- | ----------------------------- | ------------------------------ | ------------------------------ | ------------------------------
| 0.25 | ![mosample_960_025](https://user-images.githubusercontent.com/3614412/134065919-991ff82e-ef79-45d7-8fd6-477f4d268580.png) | ![mosample_1920_025](https://user-images.githubusercontent.com/3614412/134065963-dc0acd84-ed73-4beb-8c55-7fa8d30973a0.png) | ![mosample_3840_025](https://user-images.githubusercontent.com/3614412/134065971-23085cbf-567c-409b-a426-a0352a2e921c.png) | ![mosample_7680_025](https://user-images.githubusercontent.com/3614412/134065978-e78c865a-f921-4743-9889-988700b0291d.png)
| 0.50 | ![mosample_960_050](https://user-images.githubusercontent.com/3614412/134065956-9b5a75d1-3c41-4dc0-b4fb-c9787c63bbeb.png) | ![mosample_1920_050](https://user-images.githubusercontent.com/3614412/134065965-bd1dba93-cd04-4c2c-880a-23907cb823a6.png) | ![mosample_3840_050](https://user-images.githubusercontent.com/3614412/134065972-24d4ce15-7528-4fdc-9ee2-509aa6cbc9fc.png) | ![mosample_7680_050](https://user-images.githubusercontent.com/3614412/134065979-2c158e36-03a2-46cb-bd5d-b461a9580eef.png)
| 0.75 | ![mosample_960_075](https://user-images.githubusercontent.com/3614412/134065958-f2f9a2ed-ac75-44e6-a23d-ff8bd845db74.png) | ![mosample_1920_075](https://user-images.githubusercontent.com/3614412/134065967-51956d12-c611-4365-85a3-d4b0841cd8b0.png) | ![mosample_3840_075](https://user-images.githubusercontent.com/3614412/134065975-290e4508-2b02-4336-8b67-310f9a8b6ef8.png) | ![mosample_7680_075](https://user-images.githubusercontent.com/3614412/134065981-4474b397-e073-465f-8e4e-776031c3994f.png)
| 1.00 | ![mosample_960_100](https://user-images.githubusercontent.com/3614412/134065959-919d64e8-29b4-4d08-96bc-6e9c323082c4.png) | ![mosample_1920_100](https://user-images.githubusercontent.com/3614412/134065969-bef4d03d-3cc2-490e-bdd2-ab17db41978f.png) | ![mosample_3840_100](https://user-images.githubusercontent.com/3614412/134065977-9ed70fba-a8af-4e67-92cc-ca02b5d7bf5a.png) | ![mosample_7680_100](https://user-images.githubusercontent.com/3614412/134065982-991ba3c1-5b1a-4aef-8f5b-b54abf68cc47.png)

## Building
1. Extract [ffmpeg-n5.1-latest-win64-gpl-shared-5.1](https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n5.1-latest-win64-gpl-shared-5.1.zip) to `deps\ffmpeg\`.
2. Copy contents of `deps\ffmpeg\bin\` to `bin\`.
3. Build `deps\minhook\build\VC16\MinHookVC16.sln` in Release.
4. Open `svr.sln`.
5. Call `build_shaders.cmd` from a Visual Studio Developer Command Prompt. In Visual Studio 2022, you can use `Tools -> Command Line -> Developer Command Prompt`.
