# Source Video Render
[Download](https://github.com/crashfort/SourceDemoRender/releases)

[Discord](https://discord.gg/5t8D68c)

Source Video Render (SVR, formely SDR) can be used to produce movies for the Source engine with high performance. The main purpose is for movement based game modes like surf that does not require video effects. If you need video effects, see [HLAE](https://www.advancedfx.org/).

SVR operates on the H264 family of codecs for video with *libx264* for YUV and *libx264rgb* for RGB. Other codecs such as *vp8, vp9, av1* can be supported but are not directly accessible.

When using `svr_launcher.exe` you are starting the standalone SVR, which modifies existing games to add SVR support. Steam must be running for the standalone launcher to work. The launcher will list all installed supported games and you can select a game to start.

## Updates
You can use `update.cmd` in the SVR directory to automatically download the latest release.

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
Once in game, you can use the `startmovie` console command to start producing a movie and `endmovie` to stop. The `startmovie` command takes 1 or 2 parameters in this format: `startmovie <name> (<profile>)`. The *name* is the filename of the movie which will be located in `data/movies`. **If the name does not contain an extension (container), mp4 will automatically be selected.**. The *profile* is an optional parameter that decides which settings this movie will use. If not specified, the default profile is used (see below about profiles).

When starting and ending a movie, the files `svr_movie_start.cfg` and `svr_movie_end.cfg` in `data/cfg` will be executed. This can be used to insert commands that should be active only during the movie period. Note that these files are **not** in the game directory.

## Interoperability with other programs
Due to the nature of reverse engineering games, it cannot be trusted that direct interoperability will work straight away because at the risk of collision. For other programs that want to make use of SVR for rendering purposes, it can be used as a library.

## Profiles
All settings are loaded from profiles which are located in `data/profiles`. The default profile is called `default.ini` and is the profile that will be used in case none is specified when starting a movie. These profiles are shared across all games. The settings of a profile is described below. All profiles are written in a simple INI format.

The default profile is used if none is specified when starting the movie. You can create your own profiles by copying `default.ini` and renaming it (with no spaces) and making your changes. When starting your movie you can then specify your new profile. See Producing above.

### Movie
| Key | Value
| --- | -----
| video_fps | The constant framerate to use for the movie. Whole numbers only.
| video_encoder | The video encoder to use for the movie. Available options are *libx264* or *libx264rgb*. For YUV video, *libx264* is used. For RGB video, *libx264rgb* is used. There may be compatibility issues with *libx264rgb* but it produces the highest quality.
| video_x264_crf | The constant rate factor to use for the movie. This is the direct link between quality and file size. Using 0 here produces lossless video, but may cause the video stream to not be supported in some media programs. This should be between 0 and 52. A lower value means better quality but larger file size.
| video_x264_preset | The quality vs speed to use for encoding the movie. This can be one of *ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo*. A slower preset will produce slightly better quality but will significantly slow down the producing process.
| video_x264_intra | This decides whether or not the resulting video stream will consist only of keyframes. This essentially disables any compression. This will greatly increase the file size, but makes video editing very fast. This should be *0* or *1*.

### Motion blur
See the [motion blur demo](#motion-blur-demo) for what these values correspond to.
| Key | Value
| --- | -----
| motion_blur_enabled | Whether or not motion blur should be enabled or not.
| motion_blur_fps_mult | How much to multiply the movie framerate with. The product of this is how many samples per second that will be processed. For example, a 60 fps movie with 60 motion blur mult becomes 3600 samples per second. This must be greater than 0.
| motion_blur_frame_exposure | Fraction of how much time per movie frame should be exposed for sampling. This should be between 0 and 1. The later half of a completed frame is considered.

### Velocity overlay
| Key | Value
| --- | -----
| velo_enabled | Whether or not the velocity overlay is enabled. The velocity overlay will show the velocity of the current player. In case of multiplayer games with spectating, it will use the spectated player. This should be *0* or *1*.
| velo_font | The font family name to use. This should be the name of a font family that is installed on the system (such as Arial. You can see the installed fonts by searching *Fonts* in Start).
| velo_font_size | The size of the font in points in 72 dpi.
| velo_color_r | The red color component between 0 and 255.
| velo_color_g | The green color component between 0 and 255.
| velo_color_b | The blue color component between 0 and 255.
| velo_color_a | The alpha color component between 0 and 255.
| velo_font_style | The style of the font. This can be one of *normal, oblique, italic*.
| velo_font_weight | The weight of the font. This is how bold or thin the font should be. It can be one of *thin, extralight, light, semilight, normal, medium, semibold, bold, extrabold, black, extrablack*.
| velo_font_stretch | How far apart the characters should be from each other. This can be one of *undefined, ultracondensed, extracondensed, condensed, semicondensed, normal, semiexpanded, expanded, extraexpanded, ultraexpanded*.
| velo_text_align | The horizontal text alignment. Can be one of *leading, trailing, center*.
| velo_paragraph_align | The vertical text alignment. Can be one of *near, far, center*.
| velo_padding | How much the text should be offset from the calculated alignment.

## Motion blur demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher samples per second will remove banding at cost of slower recording times. For slower scenes you may get away with a lower sampling rate. Exposure is dependant on the type of content being made. The goal you should be aiming for is to reduce the banding that happens with lower samples per second. A smaller exposure will leave shorter trails of motion blur.

The X axis is the samples per second and the Y axis is the exposure (click on the images to see them larger).
|      | 960                           | 1920                           | 3840                           | 7680
| ---- | ----------------------------- | ------------------------------ | ------------------------------ | ------------------------------
| 0.25 | ![](media/mosample_960_025.png) | ![](media/mosample_1920_025.png) | ![](media/mosample_3840_025.png) | ![](media/mosample_7680_025.png)
| 0.50 | ![](media/mosample_960_050.png) | ![](media/mosample_1920_050.png) | ![](media/mosample_3840_050.png) | ![](media/mosample_7680_050.png)
| 0.75 | ![](media/mosample_960_075.png) | ![](media/mosample_1920_075.png) | ![](media/mosample_3840_075.png) | ![](media/mosample_7680_075.png)
| 1.00 | ![](media/mosample_960_100.png) | ![](media/mosample_1920_100.png) | ![](media/mosample_3840_100.png) | ![](media/mosample_7680_100.png)
