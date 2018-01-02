#include "Stream.hpp"
#include <SDR Shared\Error.hpp>
#include <SDR Shared\D3D11.hpp>
#include "Interface\Application\Extensions\ExtensionManager.hpp"
#include "Interface\Application\Modules\Shared\Console.hpp"
#include "Profile.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\BGR0.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\ClearUAV.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\PassUAV.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\Sampling.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\YUV420.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\YUV444.hpp"
#include "ConversionBGR0.hpp"
#include "ConversionYUV.hpp"
#include <functional>

namespace
{
	namespace Variables
	{
		SDR::Console::Variable UseDebugDevice;

		SDR::StartupFunctionAdder A1("Stream console variables", []()
		{
			UseDebugDevice = SDR::Console::MakeBool("sdr_d3d11_debug", []()
			{
				#ifdef _DEBUG
				return "1";
				#else
				return "0";
				#endif
			}());
		});
	}
}

namespace
{
	void WarnAboutFeatureLevel(D3D_FEATURE_LEVEL level)
	{
		if (level < D3D_FEATURE_LEVEL_11_0)
		{
			SDR::Error::Make("D3D11 feature level %d not compatible, minimum is %d", level, D3D_FEATURE_LEVEL_11_0);
		}
	}
}

void SDR::Stream::SharedData::DirectX11Data::Create(int width, int height, bool sampling)
{
	auto flags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	if (Variables::UseDebugDevice.GetBool())
	{
		flags |= D3D11_CREATE_DEVICE_DEBUG;
	}

	D3D_FEATURE_LEVEL createdlevel;

	/*
		https://msdn.microsoft.com/en-us/library/windows/desktop/ff476082.aspx
		Not providing a feature level array makes it try 11.0 first and then lesser feature levels.
	*/
	Error::Microsoft::ThrowIfFailed
	(
		D3D11CreateDevice
		(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			flags,
			nullptr,
			0,
			D3D11_SDK_VERSION,
			Device.GetAddressOf(),
			&createdlevel,
			Context.GetAddressOf()
		),
		"Could not create D3D11 device"
	);

	WarnAboutFeatureLevel(createdlevel);

	/*
		Divisors must match number of threads in SharedAll.hlsl.
	*/
	GroupsX = std::ceil(width / 8.0);
	GroupsY = std::ceil(height / 8.0);

	if (sampling)
	{
		D3D11_BUFFER_DESC cbufdesc = {};
		cbufdesc.ByteWidth = sizeof(SamplingConstantData);
		cbufdesc.Usage = D3D11_USAGE_DYNAMIC;
		cbufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbufdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Error::Microsoft::ThrowIfFailed
		(
			Device->CreateBuffer(&cbufdesc, nullptr, SamplingConstantBuffer.GetAddressOf()),
			"Could not create sampling constant buffer"
		);
	}

	{
		/*
			Matches SharedInputData in SharedAll.hlsl.
		*/
		__declspec(align(16)) struct
		{
			int Dimensions[2];
		} cbufdata;

		cbufdata.Dimensions[0] = width;
		cbufdata.Dimensions[1] = height;

		D3D11_BUFFER_DESC cbufdesc = {};
		cbufdesc.ByteWidth = sizeof(cbufdata);
		cbufdesc.Usage = D3D11_USAGE_IMMUTABLE;
		cbufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		D3D11_SUBRESOURCE_DATA cbufsubdesc = {};
		cbufsubdesc.pSysMem = &cbufdata;

		Error::Microsoft::ThrowIfFailed
		(
			Device->CreateBuffer(&cbufdesc, &cbufsubdesc, SharedConstantBuffer.GetAddressOf()),
			"Could not create constant buffer for shared shader data"
		);
	}

	if (sampling)
	{
		D3D11::OpenShader(Device.Get(), "Sampling", SDR::D3D11::BlobData::Make(CSBlob_Sampling), SamplingShader.GetAddressOf());
		D3D11::OpenShader(Device.Get(), "ClearUAV", SDR::D3D11::BlobData::Make(CSBlob_ClearUAV), ClearShader.GetAddressOf());
	}

	else
	{
		D3D11::OpenShader(Device.Get(), "PassUAV", SDR::D3D11::BlobData::Make(CSBlob_PassUAV), PassShader.GetAddressOf());
	}
}

void SDR::Stream::StreamBase::DirectX9Data::SharedSurfaceData::Create(IDirect3DDevice9Ex* device, int width, int height)
{
	Error::Microsoft::ThrowIfFailed
	(
		/*
			Once shared with D3D11, it is interpreted as
			DXGI_FORMAT_B8G8R8A8_UNORM.

			Previously "CreateOffscreenPlainSurface" but that function
			produced black output for some users.

			According to MSDN (https://msdn.microsoft.com/en-us/library/windows/desktop/ff476531(v=vs.85).aspx)
			the flags for the texture needs to be RENDER_TARGET which I guess the previous function didn't set.
			MSDN also mentions an non-existent SHADER_RESOURCE flag which seems safe to omit.

			This change was first made in:
			https://github.com/crashfort/SourceDemoRender/commit/eaabd701ce413cc372aeabe57755ce37e4bf741c
		*/
		device->CreateTexture
		(
			width,
			height,
			1,
			D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			Texture.GetAddressOf(),
			&SharedHandle
		),
		"Could not create D3D9 shared texture"
	);

	Error::Microsoft::ThrowIfFailed
	(
		Texture->GetSurfaceLevel(0, Surface.GetAddressOf()),
		"Could not get D3D9 surface from texture"
	);
}

void SDR::Stream::StreamBase::DirectX9Data::Create(IDirect3DDevice9Ex* device, int width, int height)
{
	SharedSurface.Create(device, width, height);

	Error::Microsoft::ThrowIfFailed
	(
		device->GetRenderTarget(0, GameRenderTarget0.GetAddressOf()),
		"SDR: Could not get D3D9 RT\n"
	);
}

void SDR::Stream::StreamBase::DirectX11Data::Create(ID3D11Device* device, HANDLE dx9handle, const AVFrame* reference, bool staging)
{
	Microsoft::WRL::ComPtr<ID3D11Resource> tempresource;

	Error::Microsoft::ThrowIfFailed
	(
		device->OpenSharedResource(dx9handle, IID_PPV_ARGS(tempresource.GetAddressOf())),
		"Could not open shared D3D9 resource"
	);

	Error::Microsoft::ThrowIfFailed
	(
		tempresource.As(&SharedTexture),
		"Could not query shared D3D9 resource as a D3D11 2D texture"
	);

	Error::Microsoft::ThrowIfFailed
	(
		device->CreateShaderResourceView(SharedTexture.Get(), nullptr, SharedTextureSRV.GetAddressOf()),
		"Could not create SRV for D3D11 backbuffer texture"
	);

	{
		/*
			As seen in SharedAll.hlsl.
		*/
		struct WorkBufferData
		{
			float Color[3];
			float Padding;
		};

		auto px = reference->width * reference->height;
		auto size = sizeof(WorkBufferData);

		D3D11_BUFFER_DESC bufdesc = {};
		bufdesc.ByteWidth = px * size;
		bufdesc.Usage = D3D11_USAGE_DEFAULT;
		bufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufdesc.StructureByteStride = size;

		Error::Microsoft::ThrowIfFailed
		(
			device->CreateBuffer(&bufdesc, nullptr, WorkBuffer.GetAddressOf()),
			"Could not create GPU work buffer"
		);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
		uavdesc.Format = DXGI_FORMAT_UNKNOWN;
		uavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavdesc.Buffer.NumElements = px;

		Error::Microsoft::ThrowIfFailed
		(
			device->CreateUnorderedAccessView(WorkBuffer.Get(), &uavdesc, WorkBufferUAV.GetAddressOf()),
			"Could not create UAV for GPU work buffer"
		);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
		srvdesc.Format = DXGI_FORMAT_UNKNOWN;
		srvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvdesc.Buffer.NumElements = px;

		Error::Microsoft::ThrowIfFailed
		(
			device->CreateShaderResourceView(WorkBuffer.Get(), &srvdesc, WorkBufferSRV.GetAddressOf()),
			"Could not create SRV for GPU work buffer"
		);
	}

	struct RuleData
	{
		using FactoryType = std::function<std::unique_ptr<D3D11::ConversionBase>()>;

		static RuleData Make(AVPixelFormat format, const char* name, const SDR::D3D11::BlobData& blob, const FactoryType& factory)
		{
			RuleData ret;
			ret.Format = format;
			ret.ShaderName = name;
			ret.ShaderBlob = blob;
			ret.Factory = factory;

			return ret;
		}

		bool Matches(const AVFrame* ref) const
		{
			return ref->format == Format;
		}

		AVPixelFormat Format;
		SDR::D3D11::BlobData ShaderBlob;
		const char* ShaderName;
		FactoryType Factory;
	};

	auto yuvfactory = []()
	{
		return std::make_unique<D3D11::ConversionYUV>();
	};

	auto bgr0factory = []()
	{
		return std::make_unique<D3D11::ConversionBGR0>();
	};

	RuleData table[] =
	{
		RuleData::Make(AV_PIX_FMT_YUV420P, "YUV420", SDR::D3D11::BlobData::Make(CSBlob_YUV420), yuvfactory),
		RuleData::Make(AV_PIX_FMT_YUV444P, "YUV444", SDR::D3D11::BlobData::Make(CSBlob_YUV444), yuvfactory),
		RuleData::Make(AV_PIX_FMT_BGR0, "BGR0", SDR::D3D11::BlobData::Make(CSBlob_BGR0), bgr0factory),
	};

	RuleData* found = nullptr;

	for (auto&& entry : table)
	{
		if (entry.Matches(reference))
		{
			found = &entry;
			break;
		}
	}

	if (!found)
	{
		auto name = av_get_pix_fmt_name((AVPixelFormat)reference->format);
		Error::Make("No conversion rule found for \"%s\"", name);
	}

	D3D11::OpenShader(device, found->ShaderName, found->ShaderBlob, ConversionShader.GetAddressOf());

	ConversionPtr = found->Factory();
	ConversionPtr->Create(device, reference, staging);
}

void SDR::Stream::StreamBase::DirectX11Data::NewFrame(SharedData& shared, float weight)
{
	auto context = shared.DirectX11.Context.Get();

	auto srvs = { SharedTextureSRV.Get() };
	context->CSSetShaderResources(0, 1, srvs.begin());

	auto uavs = { WorkBufferUAV.Get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);

	if (shared.DirectX11.SamplingConstantData.Weight != weight)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;

		auto hr = context->Map
		(
			shared.DirectX11.SamplingConstantBuffer.Get(),
			0,
			D3D11_MAP_WRITE_DISCARD,
			0,
			&mapped
		);

		if (FAILED(hr))
		{
			Log::Warning("SDR: Could not map sampling constant buffer\n");
		}

		else
		{
			shared.DirectX11.SamplingConstantData.Weight = weight;

			std::memcpy
			(
				mapped.pData,
				&shared.DirectX11.SamplingConstantData,
				sizeof(shared.DirectX11.SamplingConstantData)
			);
		}

		context->Unmap(shared.DirectX11.SamplingConstantBuffer.Get(), 0);
	}

	auto cbufs = { shared.DirectX11.SharedConstantBuffer.Get(), shared.DirectX11.SamplingConstantBuffer.Get() };
	context->CSSetConstantBuffers(0, 2, cbufs.begin());

	context->CSSetShader(shared.DirectX11.SamplingShader.Get(), nullptr, 0);

	Dispatch(shared);

	/*
		Force processing right now. If this flush is not here
		then the queue will clear and only transmit the latest frame
		at the GPU -> CPU sync point which effectively disables the entire
		sampling effect.
	*/

	context->Flush();

	D3D11::Shader::CSResetSRV<1>(context, 0);
	D3D11::Shader::CSResetUAV<1>(context, 0);
	D3D11::Shader::CSResetCBV<2>(context, 0);
}

void SDR::Stream::StreamBase::DirectX11Data::Clear(SharedData& shared)
{
	auto context = shared.DirectX11.Context.Get();

	context->CSSetShader(shared.DirectX11.ClearShader.Get(), nullptr, 0);

	auto uavs = { WorkBufferUAV.Get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);

	auto cbufs = { shared.DirectX11.SharedConstantBuffer.Get() };
	context->CSSetConstantBuffers(0, 1, cbufs.begin());

	Dispatch(shared);

	D3D11::Shader::CSResetUAV<1>(context, 0);
	D3D11::Shader::CSResetCBV<1>(context, 0);
}

void SDR::Stream::StreamBase::DirectX11Data::Pass(SharedData& shared)
{
	auto context = shared.DirectX11.Context.Get();

	context->CSSetShader(shared.DirectX11.PassShader.Get(), nullptr, 0);

	auto srvs = { SharedTextureSRV.Get() };
	context->CSSetShaderResources(0, 1, srvs.begin());

	auto uavs = { WorkBufferUAV.Get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);

	auto cbufs = { shared.DirectX11.SharedConstantBuffer.Get() };
	context->CSSetConstantBuffers(0, 1, cbufs.begin());

	Dispatch(shared);

	D3D11::Shader::CSResetSRV<1>(context, 0);
	D3D11::Shader::CSResetUAV<1>(context, 0);
	D3D11::Shader::CSResetCBV<1>(context, 0);
}

void SDR::Stream::StreamBase::DirectX11Data::NewVideoFrame(SharedData& shared)
{
	SDR::Extension::NewVideoFrameData data;
	data.Context = shared.DirectX11.Context.Get();
	data.GameFrameUAV = WorkBufferUAV.Get();
	data.GameFrameSRV = WorkBufferSRV.Get();
	data.ConstantBuffer = shared.DirectX11.SharedConstantBuffer.Get();
	data.ThreadGroupsX = shared.DirectX11.GroupsX;
	data.ThreadGroupsY = shared.DirectX11.GroupsY;

	SDR::ExtensionManager::Events::NewVideoFrame(data);
}

void SDR::Stream::StreamBase::DirectX11Data::Conversion(SharedData& shared)
{
	auto context = shared.DirectX11.Context.Get();

	context->CSSetShader(ConversionShader.Get(), nullptr, 0);

	auto srvs = { WorkBufferSRV.Get() };
	context->CSSetShaderResources(0, 1, srvs.begin());

	auto cbufs = { shared.DirectX11.SharedConstantBuffer.Get() };
	context->CSSetConstantBuffers(0, 1, cbufs.begin());

	ConversionPtr->DynamicBind(context);

	Dispatch(shared);

	ConversionPtr->Unbind(context);

	D3D11::Shader::CSResetSRV<1>(context, 0);
	D3D11::Shader::CSResetCBV<1>(context, 0);
}

bool SDR::Stream::StreamBase::DirectX11Data::Download(SharedData& shared, FutureData& item)
{
	auto context = shared.DirectX11.Context.Get();
	return ConversionPtr->Download(context, item);
}

void SDR::Stream::StreamBase::DirectX11Data::Dispatch(const SharedData& shared)
{
	auto& dx11 = shared.DirectX11;
	dx11.Context->Dispatch(dx11.GroupsX, dx11.GroupsY, 1);
}
