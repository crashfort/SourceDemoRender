## Source Demo Render

Plugin to create movies for Source 2013 games. To install you place SourceDemoRender.dll in your mod directory and sdr_load / sdr_unload in cfg. Then type **exec sdr_load** in the console at the main menu. If you want to see all available commands you can type **cvarlist sdr_** in the console, a description is available for all variables.

**You need to launch with -insecure for Source to be able to load plugins.**

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
			Encoded pixel format to use. Not avaialble in PNG sequence.
			<br/><br/>
			<b>Values:</b> I420, I444 or NV12 <br/>
            <b>Default:</b> I420 <br/>
			<a href="https://wiki.videolan.org/YUV/">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_crf</td>
		<td>
			Constant rate factor quality value. Not avaialble in PNG sequence.
			<br/><br/>
            <b>Values:</b> 0 to 51 <br/>
            <b>Default:</b> 10 <br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_preset</td>
		<td>
			Encoding preset. If you can, prefer not to use a slow encoding profile as the encoding may fall behind and the game will have to wait for it to catch up. Not avaialble in PNG sequence.
			<br/><br/>
            <b>Default:</b> medium <br/>
			<a href="https://trac.ffmpeg.org/wiki/Encode/H.264">Read more</a>
		</td>
	</tr>
	<tr>
		<td>sdr_movie_encoder_tune</td>
		<td>
			Not avaialble in PNG sequence.
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
			is taking too long, consider not using too low of a profile. Under normal circumstances this should not be an issue.            
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
		<td>sdr_movie_x264_options</td>
		<td>
			Optional encoder options to append. Not avaialble in PNG sequence.
			<br/><br/>
			Format:
			<pre>sdr_movie_x264_options key1=value key2=value key3=value ...</pre>
			<br/>
			See available options <a href="https://www.ffmpeg.org/ffmpeg-codecs.html#Options-25">here</a> and <a href="https://www.ffmpeg.org/ffmpeg-codecs.html#Codec-Options">here</a>
		</td>
	</tr>
	</tbody>
</table>

### Instructions
When you are ready to create your movie you just type **startmovie name** and then **endmovie** as usual. There's no need to change **host_framerate** as that is done automatically.

You can create avi or mp4 videos or a png image sequence. The format is decided from the extension you choose in startmovie. Examples:

- startmovie test.avi
- startmovie test.mp4
- startmovie test%05d.png

The last instance will create a png image sequence with 5 padded digits.
