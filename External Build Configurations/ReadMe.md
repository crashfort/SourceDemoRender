# Build configurations used for external projects

## ffmpeg-20170605-4705edb
```
CC=cl ./configure \
--prefix=$HOME/ffmpegbuild \
--toolchain=msvc \
--enable-static \
--enable-gpl \
--disable-shared \
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
--enable-muxer=image2 \
--enable-protocol=file \
--enable-libx264 \
--enable-encoder=libx264 \
--enable-encoder=png \
--enable-encoder=targa \
--enable-parser=png \
--extra-ldflags="-LIBPATH:$HOME/ffmpegbuild/lib" \
--extra-cflags="-I$HOME/ffmpegbuild/include" \

```

## x264-snapshot-20170613-2245-stable
```
CC=cl ./configure \
--prefix=$HOME/ffmpegbuild \
--disable-cli \
--enable-static \
--enable-strip \
--disable-opencl \

```

## cpprestsdk
```
Static library with project casablanca140.static.vcxproj, with preprocessor definitions: 
_NO_ASYNCRTIMP
_NO_PPLXIMP
```
