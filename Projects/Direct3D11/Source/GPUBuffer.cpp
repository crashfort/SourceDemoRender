#include "SDR Shared\GPUBuffer.hpp"
#include "SDR Shared\Error.hpp"

void SDR::D3D11::GPUBuffer::Create(ID3D11Device* device, DXGI_FORMAT viewformat, size_t size, int numelements, bool staging)
{
	Size = size;
	Staging = staging;

	/*
		Staging requires two buffers, one that the GPU operates on and then
		copies into another buffer that the CPU can read.
	*/
	if (Staging)
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;

		Error::MS::ThrowIfFailed
		(
			device->CreateBuffer(&desc, nullptr, Buffer.GetAddressOf()),
			"Could not create generic GPU buffer for staging"
		);

		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		Error::MS::ThrowIfFailed
		(
			device->CreateBuffer(&desc, nullptr, BufferStaging.GetAddressOf()),
			"Could not create staging GPU read buffer"
		);
	}

	/*
		Other method only requires a single buffer that can be read by the CPU and written by the GPU.
	*/
	else
	{
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = size;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		Error::MS::ThrowIfFailed
		(
			device->CreateBuffer(&desc, nullptr, Buffer.GetAddressOf()),
			"Could not create generic GPU read buffer"
		);
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC viewdesc = {};
	viewdesc.Format = viewformat;
	viewdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	viewdesc.Buffer.NumElements = numelements;

	Error::MS::ThrowIfFailed
	(
		device->CreateUnorderedAccessView(Buffer.Get(), &viewdesc, View.GetAddressOf()),
		"Could not create UAV for generic GPU read buffer"
	);
}

HRESULT SDR::D3D11::GPUBuffer::Map(ID3D11DeviceContext* context, D3D11_MAPPED_SUBRESOURCE* mapped)
{
	if (Staging)
	{
		context->CopyResource(BufferStaging.Get(), Buffer.Get());
		return context->Map(BufferStaging.Get(), 0, D3D11_MAP_READ, 0, mapped);
	}

	return context->Map(Buffer.Get(), 0, D3D11_MAP_READ, 0, mapped);
}

void SDR::D3D11::GPUBuffer::Unmap(ID3D11DeviceContext* context)
{
	if (Staging)
	{
		context->Unmap(BufferStaging.Get(), 0);
		return;
	}

	context->Unmap(Buffer.Get(), 0);
}
