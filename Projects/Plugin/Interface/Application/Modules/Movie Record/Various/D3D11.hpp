#pragma once
#include <d3d11.h>

namespace SDR::D3D11
{
	void OpenShader(ID3D11Device* device, const char* name, const BYTE* data, size_t size, ID3D11ComputeShader** shader);
}
