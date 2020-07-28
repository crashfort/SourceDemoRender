# Source Video Render
[Download](https://github.com/crashfort/SourceDemoRender/releases)

[Discord](https://discord.gg/5t8D68c)

Source Video Render (SVR, formely SDR) can be used to produce movies for the Source engine with extreme performance.

SVR operates on the H264 family of codecs for video with *libx264* for YUV and *libx264rgb* for RGB. Other codecs such as *vp8, vp9, av1* can be supported but are not directly accessible. It is recommended that the *avi* container is not used, prefer *mp4* or *mkv*.

## Game support
| Game          | Windows
| ------------- | -----------------------
| Counter-Strike: Source           | ✔
| Half-Life 2                      | ✔
| Half-Life 2: Deathmatch          | ✔
| Half-Life 2: Episode One         | ✔
| Half-Life 2: Episode Two         | ✔
| Team Fortress 2                  | ✔
| Momentum                         | ✔
| Day of Defeat: Source            | ✔
| Garry's Mod                      | ✔
| Black Mesa                       | ✔
| Portal 1                         | ✔
| Counter-Strike: Global Offensive | ✔
| Portal 2                         | ❌¹

¹ Uses a version of d3d9 that is too old to support texture sharing.

## Limitations
- Audio: There currently is no audio support due to no demand.
- Velocity overlay: The velocity overlay is currently limited to *Counter-Strike: Source* and *Momentum*.

## Prerequisites
Any DirectX 11 (Direct3D 11.3) compatible graphics adapter with minimum of Windows 10 1909 is required. A feature support verification will occur when starting the launcher.

## Launching
Before launching for the first time you must edit `data/launcher-config.json` which specifies which games you have. It is recommended that you use forward slashes for everything in this file. Once you have added some games, start `svr_game_launcher_cli.exe` and follow the instructions. On every start, the launcher will look for application updates and automatically download the latest game config. You can disable updates by creating an empty file called `no_update` in the bin directory.

Games can automatically be started without prompting by inserting the game id as an argument. Like this: `svr_game_launcher_cli.exe mom-win`.

The launcher will return 0 on complete success, and 1 if any failure has occured either in the launching or within the game.

### How to edit the launcher config
The fields *exe-path*, *dir-path* and *args* must be filled in for each game that wants to be added. If any of these fields are empty, the game will not visible to the launcher.

SVR has to know the location of your games. The fields which need a filepath must be absolute. You can get a full absolute path by  holding `SHIFT` while right clicking a file and selecting *Copy as path*. **You must convert all backslashes to forward slashes** - this is because Windows treats backslashes as separators and backslashes are interpreted as control codes in the json files, which are not what you want.

| Key | Value
| --- | -----
| exe-path | The full absolute path to the executable for the game. Navigate to the installation of the game and get the full path to this file as noted above. <br>This is usually `hl2.exe`, `csgo.exe` or similar.
| dir-path | The full absolute path of the game directory. Navigate to the installation of the game and get the full path to the short name as noted above. <br> This is usually `cstrike`, `csgo`, `tf`, `momentum` or similar.
| args | Extra arguments to provide the start with, this is the same as the Steam launch parameters with one difference: a `-game` parameter must be specified here which contains the short name of the game. <br> This is usually `cstrike`, `csgo`, `tf`, `momentum` or similar.

## Producing
Once in game, you can use `startmovie` to start producing a movie. The `startmovie` command takes 1 or 2 parameters in this format: `startmovie <name> (<profile>)`. The *name* is the filename of the movie which will be located in `data/movies`. The *profile* is an optional parameter that decides which settings this movie will use. If not specified, the default profile is used (see below about profiles).

When starting and ending a movie, the files `svr_movie_start.cfg` and `svr_movie_end.cfg` in `data/cfg` will be executed. This can be used to insert commands that should be active only during the movie period. Note that these files are **not** in the game directory.

#### Performance

When producing with motion blur, greater performance can be achieved by minimizing the game window, either with the minimize button in the window title or by forcing to desktop with `Win + D`. This blocks out the window from being updated with thousands and thousands of frames per second.

Additional performance can be achieved by disabling the drawing of VGUI with `r_drawvgui 0`. **Note that this will also hide the console**. It can be useful to have a bind for `endmovie`, and then inside `svr_movie_end.cfg`, set `r_drawvgui` back to 1.

## Interoperability with other programs
Due to the nature of reverse engineering games, it cannot be trusted that direct interoperability will work straight away because at the risk of collision. For other programs that want to make use of SVR for rendering purposes, it can be used as a library.

## Multiprocess rendering
It is possible to launch multiple games and render different movies at once. To enable multiprocess rendering, start a movie in one game process. Now you will be able to start a new game. **If producing a movie from a demo, it is not possible to produce multiple movies from the same demo file. Create copies of the demo file and make each game instance use its own unique demo file!**

It may also be required to open a map beforehand so the required stuff is loaded properly.

## Profiles
All settings are loaded from profiles which are located in `data/profiles`. The default profile is called `default.json` and is the profile that will be used in case none is specified when starting a movie. These profiles are all shared across all games. The settings of a profile is described below.

### Movie
| Key | Value
| --- | -----
| encoding-threads | How many threads to use for encoding. If this value is 0, the value is automatically calculated to use every core on the system. In case of multiprocess rendering, this may be wanted to turn down so proper affinity can be selected across each process.
| video-fps | The constant framerate to use for the movie. Whole numbers only.
| video-encoder | The video encoder to use for the movie. Available options are *libx264* or *libx264rgb*. For YUV video, *libx264* is used. For RGB video, *libx264rgb* is used. There may be compatibility issues with *libx264rgb* but it produces the highest quality. Other settings will become invalid if this is changed alone. If this value is changed, *video-pixel-format* and *video-color-space* must be changed too.
| video-pixel-format | The pixel format to use for the movie. This option depends on which video encoder is being used. For RGB video, this must be *bgr0*. For YUV video, it can be one of *yuv420, yuv444, nv12, nv21*. It must be noted that there is a significant difference in the perception of color between RGB and YUV video. There are colors in RGB which are not representable in *yuv420, nv12 and nv21*. Gradients are also not possible to represent for these limited pixel formats.
| video-color-space | The color space to use for the movie. This option depends on which video encoder is being used. For RGB video, this should be *none*. For YUV video, it can be either *601* or *709*. For maximum compatibility, *601* is the one to use.
| video-x264-crf | The constant rate factor to use for the movie. This is the direct link between quality and file size. Using 0 here produces lossless video, but may cause the video stream to use a high requirement profile which some media players may not support. This should be between 0 and 52. Lower is better.
| video-x264-preset | The quality vs speed to use for the movie. This can be one of *ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo*.
| video-x264-intra | This decides whether or not the resulting video stream will consist only of intra frames. This essentially disables any compression by making every frame a full frame. This will greatly increasae the file size, but makes video editing very fast. This should be *true* or *false*.

### Motion blur
| Key | Value
| --- | -----
| enabled | Whether or not motion blur should be enabled or not.
| fps-mult | How much to multiply the movie framerate with. The result is how many samples per second will be processed. For example, a 60 fps movie with 60 multiplication becomes 3600 samples per second. This should be greater than 0.
| frame-exposure | Fraction of how much time per movie frame should be exposed for sampling. This should be between 0 and 1.

### Preview window
| Key | Value
| --- | -----
| enabled | Whether or not to use the preview window. The preview window shows what is being sent to the video encoder. It will start minimized when a movie is started, and will close when the movie has ended. It can otherwise be closed manually as well. This should be *true* or *false*.

### Velocity overlay
| Key | Value
| --- | -----
| enabled | Whether or not the velocity overlay is enabled. The velocity overlay will show the velocity of the current player. In case of multiplayer games with spectating, it will use the spectated player. This should be *true* or *false*.
| font-family | The font family name to use. This should be the name of a font family that is installed on the system. Installed means being part of the system font collection.
| font-size | The size of the font in points.
| color-r | The red color component between 0 and 255.
| color-g | The green color component between 0 and 255.
| color-b | The blue color component between 0 and 255.
| color-a | The alpha color component between 0 and 255.
| font-style | The style of the font. This can be one of *normal, oblique, italic*.
| font-weight | The weight of the font. This is how bold or thin the font should be. It can be one of *thin, extralight, light, semilight, normal, medium, semibold, bold, extrabold, black, extrablack*.
| font-stretch | How far apart the characters should be from each other. This can be one of *undefined, ultracondensed, extracondensed, condensed, semicondensed, normal, semiexpanded, expanded, extraexpanded, ultraexpanded*.
| text-align | The horizontal text alignment. Can be one of *leading, trailing, center*.
| paragraph-align | The vertical text alignment. Can be one of *near, far, center*.
| padding | How much padding to apply to all sides of the text content box.

## Motion blur demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher samples per second will remove banding at cost of slower recording times. For slower scenes you may get away with a lower sampling rate. Exposure is dependant on the type of content being made.

The X axis is the samples per second and the Y axis is the exposure.
|      | 960                           | 1920                           | 3840                           | 7680
| ---- | ----------------------------- | ------------------------------ | ------------------------------ | ------------------------------
| 0.25 | ![](media/sample/960_025.png) | ![](media/sample/1920_025.png) | ![](media/sample/3840_025.png) | ![](media/sample/7680_025.png)
| 0.50 | ![](media/sample/960_050.png) | ![](media/sample/1920_050.png) | ![](media/sample/3840_050.png) | ![](media/sample/7680_050.png)
| 0.75 | ![](media/sample/960_075.png) | ![](media/sample/1920_075.png) | ![](media/sample/3840_075.png) | ![](media/sample/7680_075.png)
| 1.00 | ![](media/sample/960_100.png) | ![](media/sample/1920_100.png) | ![](media/sample/3840_100.png) | ![](media/sample/7680_100.png)
