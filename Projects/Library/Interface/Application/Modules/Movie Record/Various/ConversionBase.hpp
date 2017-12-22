#pragma once
#include <SDR Shared\D3D11.hpp>
#include <SDR Shared\GPUBuffer.hpp>
#include "LAV.hpp"
#include "FutureData.hpp"

namespace SDR::D3D11
{
	/*
		Base for hardware conversion routines.
	*/
	struct ConversionBase
	{
		virtual ~ConversionBase() = default;

		virtual void Create(ID3D11Device* device, const AVFrame* reference, bool staging) = 0;

		/*
			States that need update every frame.
		*/
		virtual void DynamicBind(ID3D11DeviceContext* context) = 0;

		/*
			Try to retrieve data to CPU after an operation.
		*/
		virtual bool Download(ID3D11DeviceContext* context, Stream::FutureData& item) = 0;
	};
}
