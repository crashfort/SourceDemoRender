# Source Demo Render

The program can be downloaded [here](https://github.com/crashfort/SourceDemoRender/releases). Visit [here](https://twitch.streamlabs.com/crashfort/) if you wish to support the development.

## Installing
SDR comes in separate singleplayer and multiplayer versions which will only work with whatever SDK the game was built with. `SourceDemoRender.Multiplayer.dll` is for games such as Counter-Strike: Source and `SourceDemoRender.Singleplayer.dll` targets for example Half-Life 2.

The DLL of either SP or MP variant should go in the root game directory. Examples:

* steamapps\common\Counter-Strike Source\cstrike\
* steamapps\common\Half-Life 2\hl2\

You can use the cfg files that comes with SDR as a base where to add your own preferred settings. The loader cfg's are the preferred way to load SDR. It is executed as follows:

* `exec sdr_load_mp` - For multiplayer games
* `exec sdr_load_sp` - For singleplayer games

The plugin can be loaded at the main menu or in demo playback, but must be before any call to `startmovie`.

**You need to launch with -insecure for Source to be able to load plugins.**

## Instructions
When you are ready to create your movie just type `startmovie <name>` and then `endmovie` as usual. There's no need to change host_framerate as that is done automatically. **Do not exit the game until you see a gren message that says the movie is completed.**

Example of supported video containers:

* `startmovie test.avi`
* `startmovie test.mp4`
* `startmovie test.mov`
* `startmovie test.mkv`

The default video encoder is ``libx264``. Other available is ``libx264rgb`` which will produce an RGB video with no color loss. Note however that ``libx264rgb`` encodes slightly slower than ``libx264``.

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
			Check to see if there are any updates available. Library updates need to be manual but game configurations and shaders are updated automatically with this command.
		</td>
	</tr>
	<tr>
		<td>sdr_version</td>
		<td>
			Displays the current library, config and shader versions.
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
			Path where to save the video. The directory structure will be created if it doesn't already exist. This cannot be the root of a drive, it must be a in at least one directory. If this is empty, the output will be in the game root.
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
	<tr>
		<td>sdr_game_suppressdebug</td>
		<td>
			Prevents engine output debug messages to reach the operating system.
			<br/><br/>
			<b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 1 <br/>
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
		<td>sdr_pass_fullbright</td>
		<td>
			Do an extra pass to a separate video file that contains fullbright data. This extra video file will have the same encoding parameters as the main stream.
			<br/><br/>
			<b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 0 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder</td>
		<td>
			Desired video encoder.
			<br/><br/>
			<b>Values:</b> libx264, libx264rgb <br/>
            <b>Default:</b> libx264 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_pxformat</td>
		<td>
			Encoded pixel format to use.
			<br/><br/>
			<b>Values:</b><br/>
			<table>
				<tr>
					<td>libx264</td>
					<td>i420, i444</td>
				</tr>
				<tr>
					<td>libx264rgb</td>
					<td>bgr0</td>
				</tr>
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
</table>
	</tbody>