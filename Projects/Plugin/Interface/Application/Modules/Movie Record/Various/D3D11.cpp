#include "D3D11.hpp"
#include "SDR Shared\Error.hpp"

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BYTE* data, size_t size, ID3D11ComputeShader** shader)
{
	SDR::Error::MS::ThrowIfFailed
	(
		device->CreateComputeShader(data, size, nullptr, shader),
		"Could not create compute shader \"%s\"", name
	);
}
