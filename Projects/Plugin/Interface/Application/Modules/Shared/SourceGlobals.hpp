#pragma once
#include <d3d9.h>

namespace SDR::SourceGlobals
{
	IDirect3DDevice9Ex* GetD3D9DeviceEx();
	bool IsDrawingLoading();
}
