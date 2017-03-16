## Source Demo Render

Tool to create movies for Source 2013 games. To install you place SourceDemoRender.dll in your mod directory and sdr_load / sdr_unload in cfg. Then type **exec sdr_load** in the console at the main menu. If you want to see all available commands you can type **cvarlist sdr_** in the console, a description is available for all variables.

**You need to launch with -insecure for Source to be able to load plugins.**

### Variables

- **sdr_outputdir** - Path where to save the finished frames. UTF8 names are not supported in Source.
- **sdr_render_framerate** *(30 to 1000)* - Movie output framerate.
- **sdr_render_exposure** *(0.0 to 1.0)* - 0 to 1 fraction of a frame that is exposed for blending. [Read more](https://github.com/ripieces/advancedfx/wiki/GoldSrc%3Amirv_sample_exposure)
- **sdr_render_samplespersecond** - Game framerate in samples. [Read more](https://github.com/ripieces/advancedfx/wiki/GoldSrc%3Amirv_sample_sps)
- **sdr_render_framestrength** *(0.0 to 1.0)* - 0 to 1 fraction how much a new frame clears the previous. [Read more](https://github.com/ripieces/advancedfx/wiki/GoldSrc%3A__mirv_sample_frame_strength)
- **sdr_render_samplemethod** *(0 or 1)* - The integral approximation method. 0: Rectangle method, 1: Trapezoidal rule. [Read more](https://github.com/ripieces/advancedfx/wiki/GoldSrc%3A__mirv_sample_smethod)
- **sdr_endmovieflash** *(0 or 1)* - Flash the window when endmovie gets called. This can be used with the demo director to do "endmovie" on a certain tick so you don't have to keep looking at the window.

More details about sampling can be [read here](https://github.com/ripieces/advancedfx/wiki/GoldSrc%3ASampling-System).

#### Video variables
- **sdr_movie_encoder_pxformat** - Pixel format to use, I420, I444 or NV12. [Read more](https://wiki.videolan.org/YUV/)
- **sdr_movie_encoder_crf** *(0 to 51)* - Constant rate factor value. [Read more](https://trac.ffmpeg.org/wiki/Encode/H.264)
- **sdr_movie_encoder_preset** - X264 encoder preset. [Read more](https://trac.ffmpeg.org/wiki/Encode/H.264)
- **sdr_movie_encoder_tune** - X264 encoder tune. [Read more](https://trac.ffmpeg.org/wiki/Encode/H.264)

### Instructions
When you are ready to create your movie you just type **startmovie name** and then **endmovie** as usual. There's no need to change **host_framerate** as that is done automatically.

You can create avi, mp4 videos and png image sequences. The format is decided from the extension you choose in startmovie. Examples:

- startmovie test.avi
- startmovie test.mp4
- startmovie test%05d.png

The last instance will create a png image sequence with 5 padded digits.
