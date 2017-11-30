#include "SDR Shared\D3D11.hpp"
#include "SDR Shared\Error.hpp"

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BYTE* data, size_t size, ID3D11ComputeShader** shader)
{
	Error::MS::ThrowIfFailed
	(
		device->CreateComputeShader(data, size, nullptr, shader),
		"Could not create compute shader \"%s\"", name
	);
}

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BYTE* data, size_t size, ID3D11VertexShader** shader)
{
	Error::MS::ThrowIfFailed
	(
		device->CreateVertexShader(data, size, nullptr, shader),
		"Could not create compute shader \"%s\"", name
	);
}

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BYTE* data, size_t size, ID3D11PixelShader** shader)
{
	Error::MS::ThrowIfFailed
	(
		device->CreatePixelShader(data, size, nullptr, shader),
		"Could not create compute shader \"%s\"", name
	);
}
