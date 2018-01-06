#pragma once
#include "ConversionBase.hpp"
#include <SDR Shared\GPUBuffer.hpp>

namespace SDR::D3D11
{
	struct ConversionBGR0 : ConversionBase
	{
		virtual void Create(ID3D11Device* device, const AVFrame* reference, bool staging) override;
		virtual void DynamicBind(ID3D11DeviceContext* context) override;
		virtual void Unbind(ID3D11DeviceContext* context) override;
		virtual bool Download(ID3D11DeviceContext* context, Stream::FutureData& item) override;

		GPUBuffer Buffer;
	};
}
