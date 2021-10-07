@echo off

REM We use zip because more often than not people don't have 7zip or equivalent. Windows has built in support for opening zip.

setlocal
mkdir publish_temp\svr

copy /Y ".\bin\svr_game.dll" "publish_temp\svr\"
copy /Y ".\bin\svr_launcher.exe" "publish_temp\svr\"
copy /Y ".\bin\ffmpeg.exe" "publish_temp\svr\"
xcopy /Q /E ".\bin\data\" "publish_temp\svr\data\"
copy /Y ".\update.cmd" "publish_temp\svr\"
copy /Y ".\README.MD" "publish_temp\svr\"
del /S /Q ".\publish_temp\svr\data\SVR_LOG.TXT"

cd publish_temp

7z a -bd -aoa -bb0 -tzip -y svr.zip svr
move /Y svr.zip ..\svr.zip

cd ..
rmdir /S /Q publish_temp
