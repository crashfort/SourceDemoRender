# Build configurations used for external projects

## ffmpeg-3.4
```
CC=cl ./configure \
--prefix=$HOME/ffmpegbuild \
--toolchain=msvc \
--enable-static \
--enable-gpl \
--disable-shared \
--disable-autodetect \
--disable-swresample \
--disable-swscale \
--disable-programs \
--disable-doc \
--disable-avdevice \
--disable-postproc \
--disable-avfilter \
--disable-network \
--disable-everything \
--disable-bzlib \
--disable-iconv \
--disable-lzma \
--disable-schannel \
--disable-sdl2 \
--disable-securetransport \
--disable-xlib \
--disable-zlib \
--enable-muxer=mp4 \
--enable-muxer=avi \
--enable-muxer=matroska \
--enable-protocol=file \
--enable-libx264 \
--enable-encoder=libx264 \
--enable-encoder=libx264rgb \
--extra-ldflags="-LIBPATH:$HOME/ffmpegbuild/lib" \
--extra-cflags="-I$HOME/ffmpegbuild/include"

```

## x264-snapshot-20171125-2245
```
CC=cl ./configure \
--prefix=$HOME/ffmpegbuild \
--disable-cli \
--enable-static \
--enable-strip \
--disable-opencl

```
