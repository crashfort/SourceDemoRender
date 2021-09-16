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

REM The shader names are an SHA1 hash that describes the file name. For the pixel format conversion combination shaders this leads to hard to search names with spaces and underscores,
REM so it is easier to use and search with a fixed string.

fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_YUV420P=1 /D AVCOL_SPC_BT470BG=1 /Fo %OUTDIR%\80789c65c37c6acbf800e0a12e18e0fd4950065b
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_YUV444P=1 /D AVCOL_SPC_BT470BG=1 /Fo %OUTDIR%\cf005a5b5fc239779f3cf1e19cf2dab33e503ffc
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_NV12=1 /D AVCOL_SPC_BT470BG=1 /Fo %OUTDIR%\2a3229bd1f9d4785c87bc7913995d82dc4e09572
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_NV21=1 /D AVCOL_SPC_BT470BG=1 /Fo %OUTDIR%\58c00ca9b019bbba2dca15ec4c4c9c494ae7d842

fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_YUV420P=1 /D AVCOL_SPC_BT709=1 /Fo %OUTDIR%\38668b40da377284241635b07e22215c204eb137
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_YUV444P=1 /D AVCOL_SPC_BT709=1 /Fo %OUTDIR%\659f7e14fec1e7018a590c5a01d8169be881438a
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_NV12=1 /D AVCOL_SPC_BT709=1 /Fo %OUTDIR%\b9978bbddaf801c44f71f8efa9f5b715ada90000
fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_NV21=1 /D AVCOL_SPC_BT709=1 /Fo %OUTDIR%\5d5924a1a56d4d450743e939d571d04a82209673

fxc shaders\tex2vid.hlsl %CS_FXCOPTS% /D AV_PIX_FMT_BGR0=1 /D AVCOL_SPC_RGB=1 /Fo %OUTDIR%\b44000e74095a254ef98a2cdfcbaf015ab6c295e

fxc shaders\motion_sample.hlsl %CS_FXCOPTS% /Fo %OUTDIR%\c52620855f15b2c47b8ca24b890850a90fdc7017

fxc shaders\text.hlsl %VS_FXCOPTS% /D TEXT_VS /Fo %OUTDIR%\34e7f561dcf7ccdd3b8f1568ebdbf4299b54f07d
fxc shaders\text.hlsl %PS_FXCOPTS% /D TEXT_PS /Fo %OUTDIR%\d19a7c625d575aa72a98c63451e97e38c16112af
