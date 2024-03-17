@echo off

where /Q fxc || (
    echo This must be run in the Visual Studio Developer Command Prompt. In Visual Studio 2022, you can use Tools -^> Command Line -^> Developer Command Prompt
    exit /b
)

REM These shaders used to be compiled with D3DCompile but that sucks because you have very minimal options and you get no information on errors.
REM Before that, shaders were compiled part of the Visual Studio project but that sucks even more beacuse configuring the shaders with the configuration pages is a nightmare.
REM We use files for the outputs instead of a header containing arrays because we don't want to recompile svr_game when a shader changes.

set CS_FXCOPTS=/T cs_5_0 /E main /nologo /WX /Ges /Zpc /Qstrip_reflect /Qstrip_debug
set VS_FXCOPTS=/T vs_5_0 /E main /nologo /WX /Ges /Zpc /Qstrip_reflect /Qstrip_debug
set PS_FXCOPTS=/T ps_5_0 /E main /nologo /WX /Ges /Zpc /Qstrip_reflect /Qstrip_debug
set OUTDIR=bin\data\shaders

fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_NV12=1 /D AVCOL_SPC_BT470BG=1 /Fo %OUTDIR%\convert_nv12
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_YUV422P=1 /D AVCOL_SPC_BT470BG=1 /Fo %OUTDIR%\convert_yuv422
fxc shaders\motion_sample.hlsl %CS_FXCOPTS% /Fo %OUTDIR%\mosample
fxc shaders\downsample.hlsl %CS_FXCOPTS% /Fo %OUTDIR%\downsample
