#pragma once
#include "D3D11.hpp"

namespace SDR::D3D11
{
	/*
		Hardware conversion shaders will store their data in this type.
		It's readable by the CPU and the finished frame is expected to be in
		the right format.
	*/
	struct GPUBuffer
	{
		void Create(ID3D11Device* device, DXGI_FORMAT viewformat, size_t size, int numelements, bool staging);

		HRESULT Map(ID3D11DeviceContext* context, D3D11_MAPPED_SUBRESOURCE* mapped);
		void Unmap(ID3D11DeviceContext* context);

		bool Staging;
		size_t Size;
		Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> BufferStaging;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> View;
	};
}
