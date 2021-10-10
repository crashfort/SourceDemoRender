@echo off

where /Q curl || (
    echo Updating only supported on Windows 10 and later
    pause
    exit /b
)

echo Downloading latest SVR

REM This is a special GitHub link that points to the latest release. We always name our releases the same.
curl -# -O -L https://github.com/crashfort/SourceDemoRender/releases/latest/download/svr.zip

if errorlevel 1 (
    echo There was some error trying to download
    pause
    exit /b
)

echo Latest SVR is downloaded! Extract svr.zip and you're good to go
pause
