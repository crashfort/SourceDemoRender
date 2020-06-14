# Source Video Render
[Download](https://github.com/crashfort/SourceDemoRender/releases)

[Discord](https://discord.gg/5t8D68c)

Games that are added:
* Counter-Strike: Source
* Half-Life 2
* Half-Life 2: Deathmatch
* Half-Life 2: Episode One
* Half-Life 2: Episode Two
* Team Fortress 2
* Momentum
* Day of Defeat: Source
* Garry's Mod
* Black Mesa
* Portal 1
* Counter-Strike: Global Offensive

Known games that don't work:
* Portal 2 - *Uses old D3D9 that doesn't support texture sharing*

## Limitations
- Audio: There currently is no audio support due to no demand.
- Velocity overlay: The velocity overlay is currently limited to *Counter-Strike: Source* and *Momentum*.

## Prerequisites
Any DirectX 11 (Direct3D 11.0) compatible graphics adapter with minimum of Windows 10 1909 is required.

## Launching
Before launching for the first time you must edit `data/launcher-config.json` which specifies which games you have. It is recommended that you use forward slashes for everything in this file. Once you have added some games, start *svr_game_launcher.exe* and follow the instructions. On every start, the launcher will look for application updates and automatically download the latest game config. You can disable updates by creating an empty file called `no_update` in the bin directory.

Games can automatically be started without prompting by inserting the game id as an argument. Like this: `svr_game_launcher.exe mom-win`.

The launcher will return 0 on complete success, and 1 if any failure has occured either in the launching or within the game.

## Producing
Once in game, you can use `startmovie` to start producing a movie. The *startmovie* command takes 1 or 2 parameters in this format: `startmovie <name> (<profile>)`. The *name* is the filename of the movie which will be located in `data/movies`. The *profile* is an optional parameter that decides which settings this movie will use. If not specified, the default profile is used (see below about profiles).

## Interoperability with other programs
Due to the nature of reverse engineering games, it cannot be trusted that direct interoperability will work straight away because at the risk of collision. For other programs that want to make use of SVR for rendering purposes, it can be used as a library.

## Multiprocess rendering
It is possible to launch multiple games and render different movies at once. To enable multiprocess rendering, start a movie in one game process. Now you will be able to start a new game. **If producing a movie from a demo, it is not possible to produce multiple movies from the same demo file. Create copies of the demo file and make each game instance use its own unique demo file!**

## Profiles
All settings are saved in profiles which are located in `data/profiles`. Each file here is a profile. The default profile is called `default.json` and is the profile that will be used in case none is specified when starting a movie. These profiles are all shared across all games. The settings of a profile is described below.

### Movie
``encoding-threads`` - How many threads to use for encoding. If this value is 0, the value is automatically calculated. In case of multiprocess rendering, this may be wanted to turn down so proper affinity can be selected across each process.

``video-fps`` - The constant framerate to use for the movie.

``video-encoder`` - The video encoder to use for the movie. Available options are *libx264* or *libx264rgb*. For YUV video, *libx264* is used. For RGB video, *libx264rgb* is used. There may be compatibility issues with *libx264rgb* but it produces the highest quality. Other settings will become invalid if this is changed alone. If this value is changed, *video-pixel-format* and *video-color-space* must be changed too.

``video-pixel-format`` - The pixel format to use for the movie. This option depends on which video encoder is being used. For RGB video, this must be *bgr0*. For YUV video, it can be one of *yuv420*, *yuv444*, *nv12*, *nv21*. It must be noted that there is a significant difference in the perception of color between RGB and YUV video. There are colors in RGB which are not representable in *yuv420*, *nv12* and *nv21*. Gradients are also not possible to represent for these limited pixel formats.

``video-color-space`` - The color space to use for the movie. This option depends on which video encoder is being used. For RGB video, this should be *none*. For YUV video, it can be either *601* or *709*. For maximum compatibility, *601* is the one to use.

``video-x264-crf`` - The constant rate factor to use for the movie. This is the direct link between quality and file size. Using 0 here produces lossless video, but may cause the video stream to use a high requirement profile which some media players may not support. This should be between 0 and 52.

``video-x264-preset`` - The quality vs speed to use for the movie. This can be one of *ultrafast*, *superfast*, *veryfast*, *faster*, *fast*, *medium*, *slow*, *slower*, *veryslow*, *placebo*.

``video-x264-intra`` - This decides whether or not the resulting video stream will consist only of intra frames. This essentially disables any compression by making every frame a full frame. This will greatly increasae the file size, but makes video editing very fast. This should be *true* or *false*.

### Motion blur
``enabled`` - Whether or not motion blur should be enabled or not. There are some internal optimizations that can happen if this is disabled. This should be *true* or *false*.

``fps-mult`` - How much to multiply the movie framerate with. The result is how many samples per second will be processed. For example, a 60 fps movie with 60 multiplication becomes 3600 samples per second. This should be greater than 0.

``frame-exposure`` - Fraction of how much time per movie frame should be exposed for sampling. This should be between 0 and 1.

### Preview window
``enabled`` - Whether or not to use the preview window. The preview window shows what is being sent to the video encoder. It will start minimized when a movie is started, and will close when the movie has ended. It can otherwise be closed manually as well. This should be *true* or *false*.

### Velocity overlay
``enabled`` - Whether or not the velocity overlay is enabled. The velocity overlay will show the velocity of the current player. In case of multiplayer games with spectating, it will use the spectated player. This should be *true* or *false*.

``font-family`` - The font family name to use. This should be the name of a font family that is installed on the system. Installed means being part of the system font collection.

``font-size`` - The size of the font in points.

``color-r`` - The red color component between 0 and 255.

``color-g`` - The green color component between 0 and 255.

``color-b`` - The blue color component between 0 and 255.

``color-a`` - The alpha color component between 0 and 255.

``font-style`` - The style of the font. This can be one of *normal*, *oblique*, *italic*.

``font-weight`` - The weight of the font. This is how bold or thin the font should be. It can be one of *thin*, *extralight*, *light*, *semilight*, *normal*, *medium*, *semibold*, *bold*, *extrabold*, *black*, *extrablack*.

``font-stretch`` - How far apart the characters should be from each other. This can be one of *undefined*, *ultracondensed*, *extracondensed*, *condensed*, *semicondensed*, *normal*, *semiexpanded*, *expanded*, *extraexpanded*, *ultraexpanded*.

``text-align`` - The horizontal text alignment. Can be one of *leading*, *trailing*, *center*.

``paragraph-align`` - The vertical text alignment. Can be one of *near*, *far*, *center*.

``padding`` - How much padding to apply to all sides of the text content box.

## Motion blur demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher samples per second will remove banding at cost of slower recording times. For slower scenes you may get away with a lower sampling rate. Exposure is dependant on what type of scene you wish to convey.

The X axis is the samples per second and the Y axis is the exposure.
<table>
	<thead>
		<tr>
			<td></td>
			<th>960</th>
			<th>1920</th>
			<th>3840</th>
			<th>7680</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<th>0.25</th>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/960_025.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/1920_025.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/3840_025.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/7680_025.png"/></td>
		</tr>
		<tr>
			<th>0.50</th>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/960_050.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/1920_050.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/3840_050.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/7680_050.png"/></td>
		</tr>
		<tr>
			<th>0.75</th>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/960_075.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/1920_075.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/3840_075.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/7680_075.png"/></td>
		</tr>
		<tr>
			<th>1.00</th>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/960_100.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/1920_100.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/3840_100.png"/></td>
			<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/media/sample/7680_100.png"/></td>
		</tr>
	</tbody>
</table>
