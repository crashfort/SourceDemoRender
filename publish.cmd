@echo off

REM We use zip because more often than not people don't have 7zip or equivalent. Windows has built in support for opening zip.
REM You need 7z in your PATH to run this.

setlocal
mkdir publish_temp\svr

copy /Y ".\bin\svr_game.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_game64.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_standalone.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_standalone64.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_launcher.exe" "publish_temp\svr\"
copy /Y ".\bin\svr_launcher64.exe" "publish_temp\svr\"
copy /Y ".\bin\svr_encoder.exe" "publish_temp\svr\"
copy /Y ".\bin\svr_log.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_log64.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_console.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_console64.dll" "publish_temp\svr\"
copy /Y ".\bin\avcodec-59.dll" "publish_temp\svr\"
copy /Y ".\bin\avdevice-59.dll" "publish_temp\svr\"
copy /Y ".\bin\avfilter-8.dll" "publish_temp\svr\"
copy /Y ".\bin\avformat-59.dll" "publish_temp\svr\"
copy /Y ".\bin\avutil-57.dll" "publish_temp\svr\"
copy /Y ".\bin\postproc-56.dll" "publish_temp\svr\"
copy /Y ".\bin\swresample-4.dll" "publish_temp\svr\"
xcopy /Q /E ".\bin\data\" "publish_temp\svr\data\"
copy /Y ".\update.cmd" "publish_temp\svr\"
copy /Y ".\README.MD" "publish_temp\svr\"
mkdir ".\publish_temp\svr\movies"
del /S /Q ".\publish_temp\svr\data\SVR_LOG.TXT"
del /S /Q ".\publish_temp\svr\data\ENCODER_LOG.TXT"

cd publish_temp

7z a -bd -aoa -bb0 -tzip -y svr.zip svr
move /Y svr.zip ..\svr.zip

cd ..
rmdir /S /Q publish_temp
