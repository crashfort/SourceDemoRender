#pragma once
#include "ConversionBase.hpp"
#include "GPUBuffer.hpp"

namespace SDR::D3D11
{
	struct ConversionYUV : ConversionBase
	{
		virtual void Create(ID3D11Device* device, const AVFrame* reference, bool staging) override;
		virtual void DynamicBind(ID3D11DeviceContext* context) override;
		virtual bool Download(ID3D11DeviceContext* context, Stream::FutureData& item) override;

		GPUBuffer Y;
		GPUBuffer U;
		GPUBuffer V;

		Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
	};
}
