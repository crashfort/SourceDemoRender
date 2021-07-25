@echo off

REM We use zip because more often than not people don't have 7zip or equivalent. Windows has built in support for opening zip.

del svr.zip
7z a -bd -aoa -bb0 -tzip -y svr.zip bin\svr_game.dll bin\svr_launcher.exe bin\ffmpeg.exe bin\data\ update.cmd -xr!SVR_LOG.TXT
7z rn svr.zip bin svr
