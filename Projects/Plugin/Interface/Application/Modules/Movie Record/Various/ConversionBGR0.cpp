#include "ConversionBGR0.hpp"
#include "Profile.hpp"
#include "SDR Shared\Log.hpp"

namespace
{
	namespace LocalProfiling
	{
		int PushRGB;

		SDR::PluginStartupFunctionAdder A1("RGB profiling", []()
		{
			PushRGB = SDR::Profile::RegisterProfiling("PushRGB");
		});
	}
}

void SDR::D3D11::ConversionBGR0::Create(ID3D11Device* device, AVFrame* reference, bool staging)
{
	auto size = reference->linesize[0] * reference->height;
	auto count = size / sizeof(uint32_t);

	Buffer.Create(device, DXGI_FORMAT_R32_UINT, size, count, staging);
}

void SDR::D3D11::ConversionBGR0::DynamicBind(ID3D11DeviceContext* context)
{
	auto uavs = { Buffer.View.Get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);
}

bool SDR::D3D11::ConversionBGR0::Download(ID3D11DeviceContext* context, Stream::FutureData& item)
{
	Profile::ScopedEntry e1(LocalProfiling::PushRGB);

	D3D11_MAPPED_SUBRESOURCE mapped;

	auto hr = Buffer.Map(context, &mapped);

	if (FAILED(hr))
	{
		Log::Warning("SDR: Could not map D3D11 RGB buffer\n"s);
	}

	else
	{
		auto ptr = (uint8_t*)mapped.pData;
		item.Planes[0].assign(ptr, ptr + Buffer.Size);
	}

	Buffer.Unmap(context);

	return SUCCEEDED(hr);
}
