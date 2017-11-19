# Source Demo Render
The program can be downloaded [here](https://github.com/crashfort/SourceDemoRender/releases). Visit [here](https://twitch.streamlabs.com/crashfort/) if you wish to support the development.

SDR offers the highest possible performance for recording movies by utilizing the GPU and all available threads. As opposed to other Source recording methods, SDR integrates itself into the engine and runs from the inside. Frames are kept on the GPU for processing which allows maximum performance. Many frames can be sampled together to form a good motion blur effect.

## Is my game supported?
Since games might need special setup, they have to be added manually. You can request your game [here](https://github.com/crashfort/SourceDemoRender/issues) or in [Discord](https://discord.gg/5t8D68c) for it to be added.

Games that are added:
* Counter-Strike: Source
* Half-Life 2
* Half-Life 2: Episode One
* Half-Life 2: Episode Two
* Team Fortress 2
* Momentum
* Day of Defeat: Source
* Garry's Mod

Known games that don't work:
* Counter-Strike: Global Offensive - *Uses stoneage D3D9 that doesn't support texture sharing*
* Portal 2 - *Same as above*

## Prerequisites
Any DirectX 11 (Direct3D 11.0) compatible adapter with minimum of Windows 7 is required. If you wish to not use **sdr_d3d11_staging**, Windows 8.1 or later is required.

## Installing
The content of the archive should go in the root game directory. Examples:

* steamapps\common\Counter-Strike Source\cstrike\
* steamapps\common\Half-Life 2\hl2\
* steamapps\sourcemods\momentum\

After you've extracted the archive you should navigate into the SDR directory and run the updater. It downloads the required game config data that is needed for game support.

## Launching

### Batch files
The launcher ``LauncherCLI.exe`` takes the following parameters: ``<exe path> <startup params ...>``. To get the executable path you hold shift and right click the file then select ``Copy as path``, the quotes are required. Anything after will be passed to the game. Automatically appended parameters are ``-steam -insecure +sv_lan 1 -console``. Examples of executable paths:

* steamapps\common\Counter-Strike Source\hl2.exe
* steamapps\common\Half-Life 2\hl2.exe
* steamapps\common\Source SDK Base 2013 Multiplayer\hl2.exe

You can use ``LauncherCLI User.bat`` to aid launching. Edit the content to fit your game.

### User interface
The launcher ``LauncherUI.exe`` comes as a separate download on the releases page. This launcher does not have to be in a specific game directory and can be run from anywhere. When adding a new game, you have to specify its SDR directory along with its executable file path. Games are saved and can quickly be selected another time.

**You must run as administrator.**

![Launcher UI Image 1](https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Launcher%20UI/MainWindow.png)

## Instructions
When you are ready to create your movie just type `startmovie <name>.<container>` and then `endmovie` as usual. **Do not exit the game until you see a green message that says the movie is completed.**

Example of supported video containers:

* `startmovie test.avi`
* `startmovie test.mp4`
* `startmovie test.mov`
* `startmovie test.mkv`

The default video encoder is ``libx264rgb`` which will produce an RGB video with no color loss. Other available is ``libx264`` for YUV video. Note however that ``libx264rgb`` encodes slower than ``libx264`` and will greatly increase file size.

## Guide
SDR can output in YUV420, YUV444 and BGR0 formats with x264. Color space can be `601` or `709`, for YUV video the color range is `full`.

### Vegas Pro
This video editor cannot open:
* YUV444 or RGB video - *Use YUV420 or workaround for RGB below*
* MKV containers - *Use MP4*
* Vegas Pro 14: AVI containers with YUV video - *Use MP4*
* Vegas Pro 14: `CRF 0` with `ultrafast` - *Use higher CRF or slower preset*

If you wish to open an RGB video you have to do a workaround:

* Get [ffmpeg](https://ffmpeg.org/)
* Run `.\ffmpeg.exe -i .\input.mp4 -vcodec huffyuv -pix_fmt rgb24 output.avi`
* Above will create a VFW compatible file that [VirtualDub](http://virtualdub.org/) can open
* Save with optional lossless encoder (not Lagarith) or uncompressed and open in Vegas Pro

If you just use YUV420 you have to use the `709` color space. If you also want to render using `x264vfw`, you have to set these advanced parameters to not lose any color:

`--colormatrix=bt709 --transfer=bt709 --colorprim=bt709 --range=pc`

### Adobe Premiere
This video editor can open `libx264rgb` videos natively which is the recommended way as there are no possibilities of color loss. If you want to use YUV video you should use the `601` color space.

### Kdenlive
This video editor can open everything SDR outputs and has detailed advanced settings for rendering.

## General commands
<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_update</td>
		<td>
			Check to see if there are any updates available. Library updates need to be manual.
		</td>
	</tr>
	<tr>
		<td>sdr_version</td>
		<td>
			Displays the current library version.
		</td>
	</tr>
	<tr>
		<td>sdr_accept</td>
		<td>
			Context specific user agreement to perform an action.
		</td>
	</tr>
	</tbody>
</table>

## General variables
<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_outputdir</td>
		<td>
			Path where to save the video. The directory structure must exist. This cannot be the root of a drive, it must be a in at least one directory. If this is empty, the output will be in the SDR folder.
		</td>
	</tr>
	<tr>
		<td>sdr_render_framerate</td>
		<td>
			Movie output framerate.
            <br/><br/>
            <b>Values:</b> 30 to 1000 <br/>
            <b>Default:</b> 60 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_endmovieflash</td>
		<td>
			Flash the window when endmovie gets called. This can be used with the demo director to do "endmovie" on a certain tick so you don't have to keep looking at the window.
			<br/><br/>
			<b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 0 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_endmoviequit</td>
		<td>
			Quits the game after all processing is done.
			<br/><br/>
			<b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 0 <br/>
		</td>
	</tr>
	</tbody>
</table>

## Video variables
<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_movie_suppresslog</td>
		<td>
			Enable or disable log output from LAV.
            <br/><br/>
            <b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 1 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder</td>
		<td>
			Desired video encoder.
			<br/><br/>
			<b>Values:</b> libx264, libx264rgb <br/>
            <b>Default:</b> libx264rgb <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_pxformat</td>
		<td>
			Encoded pixel format to use.
			<br/><br/>
			<b>Values:</b><br/>
			<table>
				<thead>
				<tr>
						<th>Encoder</th>
						<th>Values</th>
					</tr>
				</thead>
				<tbody>
					<tr>
						<td>libx264</td>
						<td>yuv420, yuv444</td>
					</tr>
					<tr>
						<td>libx264rgb</td>
						<td>bgr0</td>
					</tr>
				</tbody>
			</table>
			<br/>
            <b>Default:</b> First listed above per encoder <br/>
			<a href="https://wiki.videolan.org/YUV/">Read more about YUV</a>
		</td>
	</tr>
	<tr>
		<td>sdr_d3d11_staging</td>
		<td>
			Use extra intermediate buffer when retreiving data from the GPU.
			<br/><br/>
            <b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 1 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_x264_crf</td>
		<td>
			Constant rate factor quality value. Note that using 0 (lossless) can produce a video with a 4:4:4 profile which your media player might not support.
			<br/><br/>
            <b>Values:</b> 0 to 51 <br/>
            <b>Default:</b> 0 <br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_x264_preset</td>
		<td>
			Encoding preset. If you can, prefer not to use a slow encoding preset as the encoding may fall behind and the game will have to wait for it to catch up.
			<br/><br/>
            <b>Default:</b> ultrafast <br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_x264_intra</td>
		<td>
			Whether to produce a video of only keyframes.
			<br/><br/>
            <b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 1 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_colorspace</td>
		<td>
			YUV color space. This value is handled differently in media, try experimenting. Not available in RGB video.
			<br/><br/>
            <b>Values:</b> 601 or 709 <br/>
            <b>Default:</b> 709 <br/>
		</td>
	</tr>
	</tbody>
</table>

## Sampling variables
<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_sample_mult</td>
		<td>
			Value to multiply with <b>sdr_render_framerate</b>. This is how many frames will be put together to form a final frame multiplied by exposure. Less than 2 will disable sampling.
            <br/><br/>
            <b>Values:</b> Over 0 <br/>
            <b>Default:</b> 32 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_sample_exposure</td>
		<td>
			Fraction of time per frame that is exposed for sampling
            <br/><br/>
            <b>Values:</b> 0 to 1 <br/>
            <b>Default:</b> 0.5 <br/>
		</td>
	</tr>
	</tbody>
</table>

## Sampling demo
In this demo an object is rotating 6 times per second. This is a fast moving object, so higher **sdr_sample_mult** will remove banding that occurs with lower multiplications at cost of slower recording times. For slower scenes you may get away with a lower multiplication. Exposure is dependant on what type of scene you wish to convey.

The X axis is the multiplication and the Y axis is the exposure.
<table>
	<tr>
		<td></td>
		<th>16</th>
		<th>32</th>
		<th>64</th>
		<th>128</th>
	</tr>
	<tr>
		<th>0.25</th>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/16_025.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/32_025.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/64_025.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/128_025.png"/></td>
	</tr>
    <tr>
		<th>0.50</th>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/16_050.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/32_050.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/64_050.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/128_050.png"/></td>
	</tr>
    <tr>
		<th>0.75</th>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/16_075.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/32_075.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/64_075.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/128_075.png"/></td>
	</tr>
    <tr>
		<th>1.00</th>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/16_100.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/32_100.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/64_100.png"/></td>
		<td><img src="https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Media/Sampling%20Demo/128_100.png"/></td>
	</tr>
</table>

