#include <SDR Shared\D3D11.hpp>
#include <SDR Shared\Error.hpp>

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11ComputeShader** shader)
{
	Error::MS::ThrowIfFailed
	(
		device->CreateComputeShader(blob.Data, blob.Size, nullptr, shader),
		"Could not create compute shader \"%s\"", name
	);
}

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11VertexShader** shader)
{
	Error::MS::ThrowIfFailed
	(
		device->CreateVertexShader(blob.Data, blob.Size, nullptr, shader),
		"Could not create vertex shader \"%s\"", name
	);
}

void SDR::D3D11::OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11PixelShader** shader)
{
	Error::MS::ThrowIfFailed
	(
		device->CreatePixelShader(blob.Data, blob.Size, nullptr, shader),
		"Could not create pixel shader \"%s\"", name
	);
}
