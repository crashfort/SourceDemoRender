## Source Demo Render

The program can be downloaded [here](https://github.com/crashfort/SourceDemoRender/releases). Visit [here](https://twitch.streamlabs.com/crashfort/) if you wish to support the development.

### Installing
SDR comes in separate singleplayer and multiplayer versions which will only work with whatever SDK the mod was built with. `SourceDemoRender.Multiplayer.dll` is for mods such as Counter-Strike: Source and `SourceDemoRender.Singleplayer.dll` targets for example Half-Life 2.

The DLL of either SP or MP variant should go in the root mod directory. Examples:

* steamapps\common\Counter-Strike Source\cstrike\
* steamapps\common\Half-Life 2\hl2\

You can use the cfg files that comes with SDR as a base where to add your own preferred settings. The loader cfg's are the preferred way to load SDR. It is executed as follows:

* `exec sdr_load_mp` - For multiplayer games
* `exec sdr_load_sp` - For singleplayer games

The plugin can be loaded at the main menu or in demo playback, but must be before any call to `startmovie`.

**You need to launch with -insecure for Source to be able to load plugins.**

### Instructions
**sdr_outputdir must be set before starting movie!** When you are ready to create your movie just type `startmovie <name>` and then `endmovie` as usual. There's no need to change host_framerate as that is done automatically. **Do not exit the game until you see a message that says the movie is completed.**

For video you can output AVI or MP4, for image sequences there is PNG or TGA. The format is decided from the extension you choose in startmovie. Examples:

* `startmovie test.avi`
* `startmovie test.mp4`
* `startmovie test.png`
* `startmovie test.tga`

Image sequences should not have a digit specifier, it is added automatically as 5 padded digits. Note that PNG may be slow, but significantly smaller size than TGA.

If not specified, the default video encoder is x264. You can choose which video encoder you'd like to use as a second extension:

* `startmovie test.avi.huffyuv`

Above will create a huge AVI file with the huffyuv encoder.

List of available video encoders:

* libx264
* huffyuv

### General commands

<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_update</td>
		<td>
			Check to see if there is an update available. If there is a newer version, you will see a link where to get it. Nothing is downloaded.
		</td>
	</tr>
	<tr>
		<td>sdr_version</td>
		<td>
			Displays the current version.
		</td>
	</tr>
	</tbody>
</table>

### General variables

<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_outputdir</td>
		<td>
			Path where to save the video or image sequence. The directory structure will be created if it doesn't already exist.
		</td>
	</tr>
	<tr>
		<td>sdr_render_framerate</td>
		<td>
			Movie output framerate.
            <br/><br/>
            <b>Values:</b> Between 30 and 1000 <br/>
            <b>Default:</b> 60 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_render_exposure</td>
		<td>
			0 to 1 fraction of a frame that is exposed for blending.
			<br/><br/>
			<b>Values:</b> Between 0 and 1 <br/>
            <b>Default:</b> 0.5 <br/>
			<a href="https://github.com/ripieces/advancedfx/wiki/GoldSrc%3Amirv_sample_exposure">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_render_samplespersecond</td>
		<td>
			Game framerate in samples (host_framerate).
			<br/><br/>
			<b>Values:</b> See read more <br/>
            <b>Default:</b> 600 <br/>
			<a href="https://github.com/ripieces/advancedfx/wiki/GoldSrc%3Amirv_sample_sps">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_render_framestrength</td>
		<td>
			0 to 1 fraction how much a new frame clears the previous.
			<br/><br/>
			<b>Values:</b> See read more <br/>
            <b>Default:</b> 1 <br/>
			<a href="https://github.com/ripieces/advancedfx/wiki/GoldSrc%3A__mirv_sample_frame_strength">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_render_samplemethod</td>
		<td>
			The integral approximation method.
			<br/><br/>
			<b>Values:</b>
            <br/>
            <table>
                <tr>
                    <td>0</td><td>Rectangle method</td>
                </tr>
                <tr>
                    <td>1</td><td>Trapezoidal rule</td>
                </tr>
            </table>
            <br/>
            <b>Default:</b> 1 <br/>
			<a href="https://github.com/ripieces/advancedfx/wiki/GoldSrc%3A__mirv_sample_smethod">Read more</a>
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
			Quits the game after all endmovie processing is done.
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
            <b>Default:</b> 0 <br/>
		</td>
	</tr>
	</tbody>
</table>


More details about sampling can be [read here](https://github.com/ripieces/advancedfx/wiki/GoldSrc%3ASampling-System).

### Audio variables

<table>
	<thead>
		<th>Name</th>
		<th>Description</th>
	</thead>
	<tbody>
	<tr>
		<td>sdr_audio_enable</td>
		<td>
			Enable to process audio. Currently it does not include in a video file, but is placed as a separate wave file.
            <br/><br/>
            <b>Values:</b> 0 or 1 <br/>
            <b>Default:</b> 0 <br/>
		</td>
	</tr>
	</tbody>
</table>

### Video variables

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
            <b>Default:</b> 0 <br/>
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
					<td>x264</td>
					<td>I420, I444, NV12</td>
				</tr>
				<tr>
					<td>huffyuv</td>
					<td>RGB24</td>
				</tr>
				<tr>
					<td>png</td>
					<td>RGB24</td>
				</tr>
				<tr>
					<td>targa</td>
					<td>BGR24</td>
				</tr>
            </table>
			<br/>
            <b>Default:</b> First listed above per encoder <br/>
			<a href="https://wiki.videolan.org/YUV/">Read more about YUV</a>
		</td>
	</tr>
	<tr>
		<td>sdr_x264_crf</td>
		<td>
			Constant rate factor quality value. Only available for x264.
			<br/><br/>
            <b>Values:</b> 0 to 51 <br/>
            <b>Default:</b> 10 <br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_x264_preset</td>
		<td>
			Encoding preset. If you can, prefer not to use a slow encoding preset as the encoding may fall behind and the game will have to wait for it to catch up. Only available for x264.
			<br/><br/>
            <b>Default:</b> medium <br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_x264_tune</td>
		<td>
			Only available for x264.
			<br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_frame_buffersize</td>
		<td>
			How many frames that are allowed to be buffered up for encoding.
			This value can be lowered or increased depending your available RAM.
            <br/><br/>
            Keep in mind the sizes of an uncompressed RGB24 frame:
            <br/>
            <table>
				<tr>
					<td>1280x720</td>
					<td>2.7 MB</td>
				</tr>
				<tr>
					<td>1920x1080</td>
					<td>5.9 MB</td>
				</tr>
				<tr>
					<td>Calculation</td>
					<td>(((x * y) * 3) / 1024) / 1024</td>
				</tr>
            </table>
			<br/>
			Multiply the frame size with the buffer size to one that fits you.
			<br/><br/>
			The frame buffer queue will only build up and fall behind when the encoding
			is taking too long, consider not using too low of a preset. Under normal circumstances this should not be an issue.            
            <br/><br/>
            Source is a 32 bit process which limits the available RAM between 2 - 4 GB. In worst case scenario, a full buffer of 384 at 1280x720 would use 1 GB while 1920x1080 would use 2.2 GB.
            <br/><br/>
			<b>Using too high of a buffer size might eventually crash the application if there no longer is any available memory</b>
			<br/><br/>
			<b>Values:</b> Between 8 and 384 <br/>
            <b>Default:</b> 256 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_colorspace</td>
		<td>
			Media editors and players handle this value differently, try experimenting. Not available in image sequence.
			<br/><br/>
            <b>Values:</b> 601 or 709 <br/>
            <b>Default:</b> 601 <br/>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_colorrange</td>
		<td>
			Media editors and players handle this value differently, try experimenting. Not available in image sequence.
			<br/><br/>
            <b>Values:</b> partial or full <br/>
            <b>Default:</b> partial <br/>
		</td>
	</tr>
	</tbody>
</table>
