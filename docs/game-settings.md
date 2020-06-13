# Movie
``encoding-threads`` - How many threads to use for encoding. If this value is 0, the value is automatically calculated. In case of multiprocess rendering, this may be wanted to turn down so proper affinity can be selected across each process.

``video-fps`` - The constant framerate to use for the movie.

``video-encoder`` - The video encoder to use for the movie. Available options are *libx264* or *libx264rgb*. For YUV video, *libx264* is used. For RGB video, *libx264rgb* is used. There may be compatibility issues with *libx264rgb* but it produces the highest quality. Other settings will become invalid if this is changed alone. If this value is changed, *video-pixel-format* and *video-color-space* must be changed too.

``video-pixel-format`` - The pixel format to use for the movie. This option depends on which video encoder is being used. For RGB video, this must be *bgr0*. For YUV video, it can be one of *yuv420*, *yuv444*, *nv12*, *nv21*. It must be noted that there is a significant difference in the perception of color between RGB and YUV video.

``video-color-space`` - The color space to use for the movie. This option depends on which video encoder is being used. For RGB video, this should be *none*. For YUV video, it can be either *601* or *709*. For maximum compatibility, *601* is the one to use.

``video-x264-crf`` - The constant rate factor to use for the movie. This is the direct link between quality and file size. Using 0 here produces lossless video, but may cause the video stream to use a high requirement profile which some media players may not support. This should be between 0 and 52.

``video-x264-preset`` - The quality vs speed to use for the movie. It should be avoided to use a slow preset as it may take a very long time to process in real time. This can be one of *ultrafast*, *superfast*, *veryfast*, *faster*, *fast*, *medium*, *slow*, *slower*, *veryslow*, *placebo*.

``video-x264-intra`` - This decides whether or not the resulting video stream will consist only of intra frames. This essentially disables any compression by making every frame a full frame. This will greatly increasae the file size, but makes video editing very fast. This should be *true* or *false*.

# Motion blur
``enabled`` - Whether or not motion blur should be enabled or not. There are some internal optimizations that can happen if this is disabled. This should be *true* or *false*.

``fps-mult`` - How much to multiply the movie framerate with. The result is how many samples per second will be processed. For example, a 60 fps movie with 60 multiplication becomes 3600 samples per second. This should be greater than 0.

``frame-exposure`` - Fraction of how much time per movie frame should be exposed for sampling. This should be between 0 and 1.

# Preview window
``enabled`` - Whether or not to use the preview window. The preview window shows what is being sent to the video encoder. It will start minimized when a movie is started, and will close when the movie has ended. It can otherwise be closed manually as well. This should be *true* or *false*.

# Velocity overlay
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
