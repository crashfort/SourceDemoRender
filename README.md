# Source Video Render
[Download](https://github.com/crashfort/SourceDemoRender/releases)

[Discord](https://discord.gg/5t8D68c)

Source Video Render (SVR, formely SDR) can be used to produce movies for the Source engine with high performance. The main purpose is for movement based game modes that does not require video effects. If you need video effects, see [HLAE](https://www.advancedfx.org/).

SVR operates on the H264 family of codecs for video.

When using `svr_launcher.exe` you are starting the standalone SVR, which modifies existing games to add SVR support. Steam must be running for the standalone launcher to work. The launcher will list all installed supported games and you can select a game to start.

## Updates
You can use `update.cmd` in the SVR directory to automatically download the latest release. The latest SVR will be downloaded to svr.zip. You can extract this folder and it is now updated.

## Game support
| Game          | Windows
| ------------- | -----------------------
| Counter-Strike: Source           | ✔
| Counter-Strike: Global Offensive | ✔

## Limitations
- Audio: There currently is no audio support due to no demand.

## Prerequisites
Any DirectX 11 (Direct3D 11.3) compatible graphics adapter with minimum of Windows 10 1909 is required. A feature support verification will occur when starting the launcher.

## Producing
Once in game, you can use the `startmovie` console command to start producing a movie and `endmovie` to stop. The `startmovie` command takes 1 or 2 parameters in this format: `startmovie <name> (<profile>)`. The *name* is the filename of the movie which will be located in `data/`. **If the name does not contain an extension (container), mp4 will automatically be selected.**. The *profile* is an optional parameter that decides which settings this movie will use. If not specified, the default profile is used (see below about profiles).

When starting and ending a movie, the files `svr_movie_start.cfg` and `svr_movie_end.cfg` in `data/cfg` will be executed. This can be used to insert commands that should be active only during the movie period. Note that these files are **not** in the game directory. You can have game specific cfgs by using files called `svr_movie_start_user.cfg` and `svr_movie_end_user.cfg` in the game cfg folder.

## Interoperability with other programs
Due to the nature of reverse engineering games, it cannot be trusted that direct interoperability will work straight away because at the risk of collision.

## Profiles
All settings are loaded from profiles which are located in `data/profiles`. The default profile is called `default.ini` and is the profile that will be used in case none is specified when starting a movie. These profiles are shared across all games. The settings of a profile is described below. All profiles are written in a simple INI format.

The default profile is used if none is specified when starting the movie. You can create your own profiles by copying `default.ini` and renaming it and making your changes. When starting your movie you can then specify your new profile. See Producing above.

The documentation for profiles are written in `default.ini`.

## Motion blur demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher samples per second will remove banding at cost of slower recording times. For slower scenes you may get away with a lower sampling rate. Exposure is dependant on the type of content being made. The goal you should be aiming for is to reduce the banding that happens with lower samples per second. A smaller exposure will leave shorter trails of motion blur.

The X axis is the samples per second and the Y axis is the exposure (click on the images to see them larger).
|      | 960                           | 1920                           | 3840                           | 7680
| ---- | ----------------------------- | ------------------------------ | ------------------------------ | ------------------------------
| 0.25 | ![](media/mosample_960_025.png) | ![](media/mosample_1920_025.png) | ![](media/mosample_3840_025.png) | ![](media/mosample_7680_025.png)
| 0.50 | ![](media/mosample_960_050.png) | ![](media/mosample_1920_050.png) | ![](media/mosample_3840_050.png) | ![](media/mosample_7680_050.png)
| 0.75 | ![](media/mosample_960_075.png) | ![](media/mosample_1920_075.png) | ![](media/mosample_3840_075.png) | ![](media/mosample_7680_075.png)
| 1.00 | ![](media/mosample_960_100.png) | ![](media/mosample_1920_100.png) | ![](media/mosample_3840_100.png) | ![](media/mosample_7680_100.png)
