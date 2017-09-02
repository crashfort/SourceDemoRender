#include "Stream.hpp"
#include "SDR Shared\Error.hpp"
#include "D3D11.hpp"
#include "Profile.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\BGR0.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\ClearUAV.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\PassUAV.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\Sampling.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\YUV420.hpp"
#include "Interface\Application\Modules\Movie Record\Shaders\Blobs\YUV444.hpp"
#include <functional>

namespace LocalProfiling
{
	auto PushRGB = SDR::Profile::RegisterProfiling("PushRGB");
	auto PushYUV = SDR::Profile::RegisterProfiling("PushYUV");
}

void SDR::Stream::SharedData::DirectX11Data::Create(int width, int height, bool sampling)
{
	uint32_t flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
	#ifdef _DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif

	Error::MS::ThrowIfFailed
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
			nullptr,
			Context.GetAddressOf()
		),
		"Could not create D3D11 device"
	);

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

		Error::MS::ThrowIfFailed
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

		Error::MS::ThrowIfFailed
		(
			Device->CreateBuffer(&cbufdesc, &cbufsubdesc, SharedConstantBuffer.GetAddressOf()),
			"Could not create constant buffer for shared shader data"
		);
	}

	if (sampling)
	{
		D3D11::OpenShader(Device.Get(), "Sampling", CSBlob_Sampling, sizeof(CSBlob_Sampling), SamplingShader.GetAddressOf());
		D3D11::OpenShader(Device.Get(), "ClearUAV", CSBlob_ClearUAV, sizeof(CSBlob_ClearUAV), ClearShader.GetAddressOf());
	}

	else
	{
		D3D11::OpenShader(Device.Get(), "PassUAV", CSBlob_PassUAV, sizeof(CSBlob_PassUAV), PassShader.GetAddressOf());
	}
}

void SDR::Stream::StreamBase::DirectX9Data::SharedSurfaceData::Create(IDirect3DDevice9Ex* device, int width, int height)
{
	Error::MS::ThrowIfFailed
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

	Error::MS::ThrowIfFailed
	(
		Texture->GetSurfaceLevel(0, Surface.GetAddressOf()),
		"Could not get D3D9 surface from texture"
	);
}

void SDR::Stream::StreamBase::DirectX9Data::Create(IDirect3DDevice9Ex* device, int width, int height)
{
	SharedSurface.Create(device, width, height);
}

void SDR::Stream::StreamBase::DirectX11Data::GPUBuffer::Create(ID3D11Device* device, DXGI_FORMAT viewformat, int size, int numelements, bool staging)
{
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

HRESULT SDR::Stream::StreamBase::DirectX11Data::GPUBuffer::Map(ID3D11DeviceContext* context, D3D11_MAPPED_SUBRESOURCE* mapped)
{
	if (Staging)
	{
		context->CopyResource(BufferStaging.Get(), Buffer.Get());
		return context->Map(BufferStaging.Get(), 0, D3D11_MAP_READ, 0, mapped);
	}

	return context->Map(Buffer.Get(), 0, D3D11_MAP_READ, 0, mapped);
}

void SDR::Stream::StreamBase::DirectX11Data::GPUBuffer::Unmap(ID3D11DeviceContext* context)
{
	if (Staging)
	{
		context->Unmap(BufferStaging.Get(), 0);
		return;
	}

	context->Unmap(Buffer.Get(), 0);
}

void SDR::Stream::StreamBase::DirectX11Data::ConversionBGR0::Create(ID3D11Device* device, AVFrame* reference, bool staging)
{
	auto size = reference->buf[0]->size;
	auto count = size / sizeof(uint32_t);

	Buffer.Create(device, DXGI_FORMAT_R32_UINT, size, count, staging);
}

void SDR::Stream::StreamBase::DirectX11Data::ConversionBGR0::DynamicBind(ID3D11DeviceContext* context)
{
	auto uavs = { Buffer.View.Get() };
	context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);
}

bool SDR::Stream::StreamBase::DirectX11Data::ConversionBGR0::Download(ID3D11DeviceContext* context, FutureData& item)
{
	Profile::ScopedEntry e1(LocalProfiling::PushRGB);

	D3D11_MAPPED_SUBRESOURCE mapped;

	auto hr = Buffer.Map(context, &mapped);

	if (FAILED(hr))
	{
		Log::Warning("SDR: Could not map DX11 RGB buffer\n"s);
	}

	else
	{
		auto ptr = (uint8_t*)mapped.pData;
		item.Planes[0].assign(ptr, ptr + mapped.RowPitch);
	}

	Buffer.Unmap(context);

	return SUCCEEDED(hr);
}

void SDR::Stream::StreamBase::DirectX11Data::ConversionYUV::Create(ID3D11Device* device, AVFrame* reference, bool staging)
{
	auto sizey = reference->buf[0]->size;
	auto sizeu = reference->buf[1]->size;
	auto sizev = reference->buf[2]->size;

	Y.Create(device, DXGI_FORMAT_R8_UINT, sizey, sizey, staging);
	U.Create(device, DXGI_FORMAT_R8_UINT, sizeu, sizeu, staging);
	V.Create(device, DXGI_FORMAT_R8_UINT, sizev, sizev, staging);

	/*
		Matches YUVInputData in YUVShared.hlsl.
	*/
	__declspec(align(16)) struct
	{
		int Strides[3];
		int Padding1;
		float CoeffY[3];
		int Padding2;
		float CoeffU[3];
		int Padding3;
		float CoeffV[3];
	} yuvdata;

	yuvdata.Strides[0] = reference->linesize[0];
	yuvdata.Strides[1] = reference->linesize[1];
	yuvdata.Strides[2] = reference->linesize[2];

	auto setcoeffs = [](auto& obj, float x, float y, float z)
	{
		obj[0] = x;
		obj[1] = y;
		obj[2] = z;
	};

	/*
		https://msdn.microsoft.com/en-us/library/windows/desktop/ms698715.aspx
	*/

	if (reference->colorspace == AVCOL_SPC_BT470BG)
	{
		setcoeffs(yuvdata.CoeffY, +0.299000, +0.587000, +0.114000);
		setcoeffs(yuvdata.CoeffU, -0.168736, -0.331264, +0.500000);
		setcoeffs(yuvdata.CoeffV, +0.500000, -0.418688, -0.081312);
	}

	else if (reference->colorspace == AVCOL_SPC_BT709)
	{
		setcoeffs(yuvdata.CoeffY, +0.212600, +0.715200, +0.072200);
		setcoeffs(yuvdata.CoeffU, -0.114572, -0.385428, +0.500000);
		setcoeffs(yuvdata.CoeffV, +0.500000, -0.454153, -0.045847);
	}

	else
	{
		Error::Make("No matching YUV color space for coefficients"s);
	}

	D3D11_BUFFER_DESC cbufdesc = {};
	cbufdesc.ByteWidth = sizeof(yuvdata);
	cbufdesc.Usage = D3D11_USAGE_IMMUTABLE;
	cbufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA cbufsubdesc = {};
	cbufsubdesc.pSysMem = &yuvdata;

	Error::MS::ThrowIfFailed
	(
		device->CreateBuffer(&cbufdesc, &cbufsubdesc, ConstantBuffer.GetAddressOf()),
		"Could not create constant buffer for YUV GPU buffer"
	);
}

void SDR::Stream::StreamBase::DirectX11Data::ConversionYUV::DynamicBind(ID3D11DeviceContext* context)
{
	auto cbufs = { ConstantBuffer.Get() };
	context->CSSetConstantBuffers(1, 1, cbufs.begin());

	auto uavs = { Y.View.Get(), U.View.Get(), V.View.Get() };
	context->CSSetUnorderedAccessViews(0, 3, uavs.begin(), nullptr);
}

bool SDR::Stream::StreamBase::DirectX11Data::ConversionYUV::Download(ID3D11DeviceContext* context, FutureData& item)
{
	Profile::ScopedEntry e1(LocalProfiling::PushYUV);

	D3D11_MAPPED_SUBRESOURCE mappedy;
	D3D11_MAPPED_SUBRESOURCE mappedu;
	D3D11_MAPPED_SUBRESOURCE mappedv;

	auto hrs =
	{
		Y.Map(context, &mappedy),
		U.Map(context, &mappedu),
		V.Map(context, &mappedv)
	};

	bool pass = true;

	for (auto res : hrs)
	{
		if (FAILED(res))
		{
			pass = false;

			Log::Warning("SDR: Could not map DX11 YUV buffers\n"s);
			break;
		}
	}

	if (pass)
	{
		auto ptry = (uint8_t*)mappedy.pData;
		auto ptru = (uint8_t*)mappedu.pData;
		auto ptrv = (uint8_t*)mappedv.pData;

		item.Planes[0].assign(ptry, ptry + mappedy.RowPitch);
		item.Planes[1].assign(ptru, ptru + mappedu.RowPitch);
		item.Planes[2].assign(ptrv, ptrv + mappedv.RowPitch);
	}

	Y.Unmap(context);
	U.Unmap(context);
	V.Unmap(context);

	return pass;
}

void SDR::Stream::StreamBase::DirectX11Data::Create(ID3D11Device* device, HANDLE dx9handle, AVFrame* reference, bool staging)
{
	Microsoft::WRL::ComPtr<ID3D11Resource> tempresource;

	Error::MS::ThrowIfFailed
	(
		device->OpenSharedResource(dx9handle, IID_PPV_ARGS(tempresource.GetAddressOf())),
		"Could not open shared D3D9 resource"
	);

	Error::MS::ThrowIfFailed
	(
		tempresource.As(&SharedTexture),
		"Could not query shared D3D9 resource as a D3D11 2D texture"
	);

	Error::MS::ThrowIfFailed
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
		bufdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		bufdesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		bufdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		bufdesc.StructureByteStride = size;

		Error::MS::ThrowIfFailed
		(
			device->CreateBuffer(&bufdesc, nullptr, WorkBuffer.GetAddressOf()),
			"Could not create GPU work buffer"
		);

		D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc = {};
		uavdesc.Format = DXGI_FORMAT_UNKNOWN;
		uavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavdesc.Buffer.NumElements = px;

		Error::MS::ThrowIfFailed
		(
			device->CreateUnorderedAccessView(WorkBuffer.Get(), &uavdesc, WorkBufferUAV.GetAddressOf()),
			"Could not create UAV for GPU work buffer"
		);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
		srvdesc.Format = DXGI_FORMAT_UNKNOWN;
		srvdesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvdesc.Buffer.NumElements = px;

		Error::MS::ThrowIfFailed
		(
			device->CreateShaderResourceView(WorkBuffer.Get(), &srvdesc, WorkBufferSRV.GetAddressOf()),
			"Could not create SRV for GPU work buffer"
		);
	}

	struct RuleData
	{
		using FactoryType = std::function<std::unique_ptr<ConversionBase>()>;

		RuleData
		(
			AVPixelFormat format,
			const char* name,
			const BYTE* data,
			size_t datasize,
			const FactoryType& factory
		) :
			Format(format),
			ShaderName(name),
			Data(data),
			DataSize(datasize),
			Factory(factory)
		{

		}

		bool Matches(const AVFrame* ref) const
		{
			return ref->format == Format;
		}

		AVPixelFormat Format;
		const BYTE* Data;
		size_t DataSize;
		const char* ShaderName;
		FactoryType Factory;
	};

	auto yuvfactory = []()
	{
		return std::make_unique<ConversionYUV>();
	};

	auto bgr0factory = []()
	{
		return std::make_unique<ConversionBGR0>();
	};

	RuleData table[] =
	{
		RuleData(AV_PIX_FMT_YUV420P, "YUV420", CSBlob_YUV420, sizeof(CSBlob_YUV420), yuvfactory),
		RuleData(AV_PIX_FMT_YUV444P, "YUV444", CSBlob_YUV444, sizeof(CSBlob_YUV444), yuvfactory),
		RuleData(AV_PIX_FMT_BGR0, "BGR0", CSBlob_BGR0, sizeof(CSBlob_BGR0), bgr0factory),
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

	D3D11::OpenShader(device, found->ShaderName, found->Data, found->DataSize, ConversionShader.GetAddressOf());

	ConversionPtr = found->Factory();
	ConversionPtr->Create(device, reference, staging);
}

void SDR::Stream::StreamBase::DirectX11Data::ResetShaderInputs(ID3D11DeviceContext* context)
{
	/*
		At most, 3 slots are used for the YUV buffers.
	*/
	const auto count = 3;

	ID3D11ShaderResourceView* srvs[count] = {};
	ID3D11UnorderedAccessView* uavs[count] = {};
	ID3D11Buffer* cbufs[count] = {};

	context->CSSetShaderResources(0, count, srvs);
	context->CSSetUnorderedAccessViews(0, count, uavs, nullptr);
	context->CSSetConstantBuffers(0, count, cbufs);
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
			Log::Warning("SDR: Could not map sampling constant buffer\n"s);
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

	auto cbufs =
	{
		shared.DirectX11.SharedConstantBuffer.Get(),
		shared.DirectX11.SamplingConstantBuffer.Get()
	};

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

	ResetShaderInputs(context);
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

	ResetShaderInputs(context);
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

	ResetShaderInputs(context);
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

	ResetShaderInputs(context);
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
