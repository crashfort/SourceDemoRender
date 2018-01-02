#include <SDR Extension\Extension.hpp>
#include <SDR Shared\Error.hpp>
#include <SDR Shared\D3D11.hpp>
#include <SDR Shared\Table.hpp>
#include <SDR Direct2DContext API\Direct2DContextAPI.hpp>
#include <vector>
#include <memory>

#include <wrl.h>

#include "Shaders\Blobs\OverlayUAV.hpp"

namespace
{
	SDR::Extension::ImportData Import;
}

namespace
{
	namespace Variables
	{
		uint32_t AntiAlias;
		uint32_t AntiAliasMode;
	}
}

namespace
{
	struct ExtensionUserData
	{
		SDR::Direct2DContext::Direct2DContext_StartMovie StartMovie;
		SDR::Direct2DContext::Direct2DContext_NewVideoFrame NewVideoFrame;
	};

	std::vector<ExtensionUserData> Users;

	bool HasUsers()
	{
		return Users.size() > 0;
	}

	struct LocalData
	{
		void Create(const SDR::Extension::StartMovieData& data)
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = data.Width;
			desc.Height = data.Height;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

			SDR::Error::Microsoft::ThrowIfFailed
			(
				data.Device->CreateTexture2D(&desc, nullptr, Direct2DTexture.GetAddressOf()),
				"Could not create texture for Direct2D usage"
			);

			D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
			srvdesc.Format = desc.Format;
			srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvdesc.Texture2D.MipLevels = 1;

			SDR::Error::Microsoft::ThrowIfFailed
			(
				data.Device->CreateShaderResourceView(Direct2DTexture.Get(), &srvdesc, Direct2DTextureSRV.GetAddressOf()),
				"Could not create SRV for Direct2D texture"
			);

			SDR::D3D11::OpenShader(data.Device, "OverlayUAV", SDR::D3D11::MakeBlob(CSBlob_OverlayUAV), OverlayShader.GetAddressOf());

			Microsoft::WRL::ComPtr<IDXGISurface> surface;

			SDR::Error::Microsoft::ThrowIfFailed
			(
				Direct2DTexture.As(&surface),
				"Could not query texture as a IDXGISurface"
			);

			D2D1_CREATION_PROPERTIES props;
			props.options = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;
			props.threadingMode = D2D1_THREADING_MODE_SINGLE_THREADED;
			props.debugLevel = D2D1_DEBUG_LEVEL_NONE;

			SDR::Error::Microsoft::ThrowIfFailed
			(
				D2D1CreateDeviceContext(surface.Get(), props, Direct2DContext.GetAddressOf()),
				"Could not create Direct2D device context"
			);

			auto useaa = Import.GetBool(Variables::AntiAlias);

			if (useaa)
			{
				Direct2DContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
			}

			else
			{
				Direct2DContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			}

			auto aamode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
			{
				auto table =
				{
					std::make_pair("default", D2D1_TEXT_ANTIALIAS_MODE_DEFAULT),
					std::make_pair("cleartype", D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE),
					std::make_pair("grayscale", D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE)
				};

				SDR::Table::LinkToVariable(Import.GetString(Variables::AntiAliasMode), table, aamode);
			}

			Direct2DContext->SetTextAntialiasMode(aamode);

			SDR::Error::Microsoft::ThrowIfFailed
			(
				DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)DirectWriteFactory.GetAddressOf()),
				"Could not create DirectWrite factory"
			);
		}

		void DrawDirect2D(const SDR::Extension::NewVideoFrameData& data)
		{
			SDR::Direct2DContext::NewVideoFrameData userdata;
			userdata.Original = data;
			userdata.Context = Direct2DContext.Get();

			Direct2DContext->BeginDraw();
			Direct2DContext->Clear(D2D1::ColorF(0, 0, 0, 0));

			for (const auto& user : Users)
			{
				user.NewVideoFrame(userdata);
			}

			Direct2DContext->EndDraw();
		}

		void DrawDirect3D11(const SDR::Extension::NewVideoFrameData& data)
		{
			data.Context->CSSetShader(OverlayShader.Get(), nullptr, 0);

			auto uavs = { data.GameFrameUAV };
			data.Context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);

			auto srvs = { Direct2DTextureSRV.Get() };
			data.Context->CSSetShaderResources(0, 1, srvs.begin());

			auto cbufs = { data.ConstantBuffer };
			data.Context->CSSetConstantBuffers(0, 1, cbufs.begin());

			data.Context->Dispatch(data.ThreadGroupsX, data.ThreadGroupsY, 1);

			SDR::D3D11::Shader::CSResetUAV<1>(data.Context, 0);
			SDR::D3D11::Shader::CSResetSRV<1>(data.Context, 0);
			SDR::D3D11::Shader::CSResetCBV<1>(data.Context, 0);
		}

		Microsoft::WRL::ComPtr<ID3D11ComputeShader> OverlayShader;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> Direct2DTexture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Direct2DTextureSRV;

		Microsoft::WRL::ComPtr<ID2D1DeviceContext> Direct2DContext;
		Microsoft::WRL::ComPtr<IDWriteFactory> DirectWriteFactory;
	};

	std::unique_ptr<LocalData> LocalPtr;
}

extern "C"
{
	__declspec(dllexport) void __cdecl SDR_Query(SDR::Extension::QueryData& query)
	{
		query.Name = "Direct2D Context";
		query.Namespace = "Direct2DContext";
		query.Author = "crashfort";
		query.Contact = "https://github.com/crashfort/";

		query.Version = 1;
	}

	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::Extension::InitializeData& data)
	{
		SDR::Error::SetPrintFormat("Direct2DContext: %s\n");
		SDR::Extension::RedirectLogOutputs(data);
	}

	__declspec(dllexport) void __cdecl SDR_Ready(const SDR::Extension::ImportData& data)
	{
		Import = data;

		auto thismodule = Import.GetExtensionModule(Import.ExtensionKey);
		auto count = Import.GetExtensionCount();

		for (size_t i = 0; i < count; i++)
		{
			auto module = Import.GetExtensionModule(i);

			if (module == thismodule)
			{
				continue;
			}

			ExtensionUserData user;

			try
			{
				SDR::Extension::FindExport(module, "Direct2DContext_StartMovie", user.StartMovie);
				SDR::Extension::FindExport(module, "Direct2DContext_NewVideoFrame", user.NewVideoFrame);
			}

			catch (bool value)
			{
				continue;
			}

			Users.emplace_back(std::move(user));
		}

		Variables::AntiAlias = Import.MakeBool("sdr_direct2dcontext_antialias", "1");
		Variables::AntiAliasMode = Import.MakeString("sdr_direct2dcontext_antialias_mode", "cleartype");
	}

	__declspec(dllexport) void __cdecl SDR_StartMovie(const SDR::Extension::StartMovieData& data)
	{
		if (!HasUsers())
		{
			return;
		}

		auto temp = std::make_unique<LocalData>();

		try
		{
			temp->Create(data);
		}

		catch (const SDR::Error::Exception& error)
		{
			return;
		}

		LocalPtr = std::move(temp);

		SDR::Direct2DContext::StartMovieData userdata;
		userdata.Original = data;
		userdata.Context = LocalPtr->Direct2DContext.Get();
		userdata.DirectWriteFactory = LocalPtr->DirectWriteFactory.Get();

		for (const auto& user : Users)
		{
			user.StartMovie(userdata);
		}
	}

	__declspec(dllexport) void __cdecl SDR_EndMovie()
	{
		LocalPtr.reset();
	}

	__declspec(dllexport) void __cdecl SDR_NewVideoFrame(const SDR::Extension::NewVideoFrameData& data)
	{
		if (!HasUsers())
		{
			return;
		}

		LocalPtr->DrawDirect2D(data);
		LocalPtr->DrawDirect3D11(data);
	}
}
