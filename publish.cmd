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
copy /Y ".\bin\svr_studio64.exe" "publish_temp\svr\"
copy /Y ".\bin\svr_shared.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_shared64.dll" "publish_temp\svr\"
copy /Y ".\bin\avcodec-62.dll" "publish_temp\svr\"
copy /Y ".\bin\avformat-62.dll" "publish_temp\svr\"
copy /Y ".\bin\avutil-60.dll" "publish_temp\svr\"
copy /Y ".\bin\swresample-6.dll" "publish_temp\svr\"
copy /Y ".\bin\mimalloc-override.dll" "publish_temp\svr\"
copy /Y ".\bin\mimalloc-redirect.dll" "publish_temp\svr\"
xcopy /Q /E ".\bin\data\" "publish_temp\svr\data\"
xcopy /Q /E ".\bin\icons\" "publish_temp\svr\icons\"
xcopy /Q /E ".\bin\locale\" "publish_temp\svr\locale\"
copy /Y ".\update.cmd" "publish_temp\svr\"
copy /Y ".\README.MD" "publish_temp\svr\"
mkdir ".\publish_temp\svr\movies"
del /S /Q ".\publish_temp\svr\data\svr_log.txt"
del /S /Q ".\publish_temp\svr\data\encoder_log.txt"
del /S /Q ".\publish_temp\svr\data\studio_log.txt"
del /S /Q ".\publish_temp\svr\data\studio_settings.bin"

cd publish_temp

7z a -bd -aoa -bb0 -tzip -y svr.zip svr
move /Y svr.zip ..\svr.zip

cd ..
rmdir /S /Q publish_temp
