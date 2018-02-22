#include <SDR Extension\Extension.hpp>
#include <SDR Shared\String.hpp>
#include <SDR Shared\Error.hpp>
#include <SDR Shared\D3D11.hpp>
#include <SDR Shared\IPC.hpp>

#include <dxgi1_2.h>

#include <thread>
#include <memory>
#include <atomic>

#include <wrl.h>

#include "Shaders\Blobs\VertexShader.hpp"
#include "Shaders\Blobs\PixelShader.hpp"

namespace
{
	namespace Synchro
	{
		struct EventData
		{
			EventData()
			{
				Event.Attach(CreateEventA(nullptr, false, false, nullptr));
			}

			void Set()
			{
				SetEvent(Get());
			}

			HANDLE Get() const
			{
				return Event.Get();
			}

			Microsoft::WRL::Wrappers::HandleT<Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits> Event;
		};

		struct Data
		{
			Data()
			{
				if (!MainReady.Event.IsValid())
				{
					SDR::Error::Microsoft::ThrowLastError("Could not create event \"MainReady\"");
				}

				if (!WindowCreated.Event.IsValid())
				{
					SDR::Error::Microsoft::ThrowLastError("Could not create event \"WindowCreated\"");
				}
			}

			EventData MainReady;
			EventData WindowCreated;
			
			Microsoft::WRL::Wrappers::CriticalSection WindowPtrCS;
		};

		std::unique_ptr<Data> Ptr;

		void Create()
		{
			Ptr = std::make_unique<Data>();
		}

		void Destroy()
		{
			Ptr.reset();
		}
	}
}

namespace
{
	namespace Window
	{
		struct WindowThreadData
		{
			void Create(HWND owner, HWND content, const SDR::Extension::StartMovieData& data)
			{
				GameWidth = data.Width;
				GameHeight = data.Height;

				Microsoft::WRL::ComPtr<IDXGIDevice1> dxgidevice;

				SDR::Error::Microsoft::ThrowIfFailed
				(
					data.Device->QueryInterface(dxgidevice.GetAddressOf()),
					"Could not query D3D11 device as a DXGI device"
				);

				Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiadapter;

				SDR::Error::Microsoft::ThrowIfFailed
				(
					dxgidevice->GetAdapter(dxgiadapter.GetAddressOf()),
					"Could not get device adapter"
				);

				Microsoft::WRL::ComPtr<IDXGIFactory2> dxgifactory;

				SDR::Error::Microsoft::ThrowIfFailed
				(
					dxgiadapter->GetParent(IID_PPV_ARGS(dxgifactory.GetAddressOf())),
					"Could not get parent of adapter"
				);

				DXGI_SWAP_CHAIN_DESC1 swapdesc = {};
				swapdesc.Width = data.Width;
				swapdesc.Height = data.Height;
				swapdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				swapdesc.SampleDesc.Count = 1;
				swapdesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
				swapdesc.BufferCount = 1;

				DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreendesc = {};
				fullscreendesc.Windowed = true;

				SDR::Error::Microsoft::ThrowIfFailed
				(
					dxgifactory->CreateSwapChainForHwnd(data.Device, content, &swapdesc, &fullscreendesc, nullptr, SwapChain.GetAddressOf()),
					"Could not create swap chain"
				);

				SDR::Error::Microsoft::ThrowIfFailed
				(
					dxgifactory->MakeWindowAssociation(owner, DXGI_MWA_NO_ALT_ENTER),
					"Could not set window association"
				);

				Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;

				SDR::Error::Microsoft::ThrowIfFailed
				(
					SwapChain->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf())),
					"Could not get buffer from swap chain"
				);

				SDR::Error::Microsoft::ThrowIfFailed
				(
					data.Device->CreateRenderTargetView(backbuffer.Get(), nullptr, RenderTargetView.GetAddressOf()),
					"Could not create render target view"
				);

				SDR::D3D11::OpenShader(data.Device, "PixelShader", SDR::D3D11::BlobData::Make(PSBlob_PixelShader), PixelShader.GetAddressOf());
				SDR::D3D11::OpenShader(data.Device, "VertexShader", SDR::D3D11::BlobData::Make(VSBlob_VertexShader), VertexShader.GetAddressOf());
			}

			void Update(const SDR::Extension::NewVideoFrameData& data)
			{
				/*
					Don't have to clear as the entire render target gets overwritten every time.
				*/

				Prepare(data.Context);
				Render(data);
				Present();
			}

			void Prepare(ID3D11DeviceContext* context)
			{
				auto rts = { RenderTargetView.Get() };
				context->OMSetRenderTargets(1, rts.begin(), nullptr);

				CD3D11_VIEWPORT viewport(0.0, 0.0, GameWidth, GameHeight);
				context->RSSetViewports(1, &viewport);
			}

			void Render(const SDR::Extension::NewVideoFrameData& data)
			{
				data.Context->IASetInputLayout(nullptr);
				data.Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

				data.Context->VSSetShader(VertexShader.Get(), nullptr, 0);
				data.Context->PSSetShader(PixelShader.Get(), nullptr, 0);

				auto srvs = { data.GameFrameSRV };
				data.Context->PSSetShaderResources(0, 1, srvs.begin());

				auto cbufs = { data.ConstantBuffer };
				data.Context->PSSetConstantBuffers(0, 1, cbufs.begin());

				data.Context->Draw(4, 0);

				SDR::D3D11::Shader::PSResetSRV<1>(data.Context, 0);
				SDR::D3D11::Shader::PSResetCBV<1>(data.Context, 0);
			}

			void Present()
			{
				auto hr = SwapChain->Present(1, 0);
			}

			int GameWidth;
			int GameHeight;

			Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;

			Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
			Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;

			Microsoft::WRL::ComPtr<IDXGISwapChain1> SwapChain;
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTargetView;
		};

		std::unique_ptr<WindowThreadData> WindowPtr;
		std::thread WindowThread;

		LRESULT CALLBACK WindowProcedureOwner(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
		{
			switch (message)
			{
				case WM_DESTROY:
				{
					PostQuitMessage(0);
					return 0;
				}

				case WM_SIZE:
				{
					int ownerwidth = LOWORD(lparam);
					int ownerheight = HIWORD(lparam);

					auto aspect = (double)WindowPtr->GameWidth / (double)WindowPtr->GameHeight;

					auto width = ownerwidth;
					auto height = std::round(width / aspect);

					if (height > ownerheight)
					{
						width = std::round(ownerheight * aspect);
						height = ownerheight;
					}

					auto posx = std::round((ownerwidth - width) / 2);
					auto posy = std::round((ownerheight - height) / 2);

					auto contentwindow = GetWindow(hwnd, GW_CHILD);

					auto flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
					SetWindowPos(contentwindow, nullptr, posx, posy, width, height, flags);

					return 0;
				}
			}

			return DefWindowProcA(hwnd, message, wparam, lparam);
		}

		void MessageLoop()
		{
			MSG msg = {};

			while (msg.message != WM_QUIT)
			{
				if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessageA(&msg);
				}

				else
				{
					WaitMessage();
				}
			}

			/*
				Window was closed, don't let the main thread try to do something with it.
			*/

			auto lock = Synchro::Ptr->WindowPtrCS.Lock();
			WindowPtr.reset();
		}

		void MakeWindow(const SDR::Extension::StartMovieData& data)
		{
			auto instance = GetModuleHandleA(nullptr);
			
			HWND owner = nullptr;
			HWND content = nullptr;

			{
				auto classname = "SDR_PREVIEW_WINDOW_OWNER_CLASS";

				WNDCLASSEX wcex = {};
				wcex.cbSize = sizeof(wcex);
				wcex.lpfnWndProc = WindowProcedureOwner;
				wcex.hInstance = instance;
				wcex.hCursor = LoadCursorA(nullptr, IDC_ARROW);
				wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
				wcex.lpszClassName = classname;

				if (!RegisterClassExA(&wcex))
				{
					auto error = GetLastError();

					if (error != ERROR_CLASS_ALREADY_EXISTS)
					{
						SDR::Error::Microsoft::ThrowLastError("Could not register owner window class");
					}
				}

				RECT rect = {};
				rect.right = data.Width;
				rect.bottom = data.Height;

				const auto style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

				AdjustWindowRect(&rect, style, false);

				auto title = "SDR Preview";
				auto posx = CW_USEDEFAULT;
				auto posy = CW_USEDEFAULT;
				auto width = rect.right - rect.left;
				auto height = rect.bottom - rect.top;

				owner = CreateWindowExA(0, classname, title, style, posx, posy, width, height, nullptr, nullptr, instance, nullptr);

				if (!owner)
				{
					SDR::Error::Microsoft::ThrowLastError("Could not create window");
				}
			}

			{
				auto classname = "SDR_PREVIEW_WINDOW_CONTENT_CLASS";

				WNDCLASSEX wcex = {};
				wcex.cbSize = sizeof(wcex);
				wcex.lpfnWndProc = DefWindowProcA;
				wcex.hInstance = instance;
				wcex.hCursor = LoadCursorA(nullptr, IDC_ARROW);
				wcex.lpszClassName = classname;

				if (!RegisterClassExA(&wcex))
				{
					auto error = GetLastError();

					if (error != ERROR_CLASS_ALREADY_EXISTS)
					{
						SDR::Error::Microsoft::ThrowLastError("Could not register content window class");
					}
				}

				RECT rect = {};
				rect.right = data.Width;
				rect.bottom = data.Height;

				auto style = WS_CHILD | WS_VISIBLE | WS_DISABLED;

				AdjustWindowRect(&rect, style, false);

				auto title = "SDR Preview Content";
				auto posx = CW_USEDEFAULT;
				auto posy = CW_USEDEFAULT;
				auto width = rect.right - rect.left;
				auto height = rect.bottom - rect.top;

				content = CreateWindowExA(0, classname, title, style, posx, posy, width, height, owner, nullptr, instance, nullptr);

				if (!content)
				{
					SDR::Error::Microsoft::ThrowLastError("Could not create window");
				}
			}

			auto temp = std::make_unique<WindowThreadData>();
			temp->Create(owner, content, data);

			WindowPtr = std::move(temp);

			ShowWindow(owner, SW_SHOWMINNOACTIVE);
		}

		void Create(const SDR::Extension::StartMovieData& data)
		{
			WindowThread = std::thread([=]()
			{
				bool fail = false;

				try
				{
					SDR::IPC::WaitForOne({ Synchro::Ptr->MainReady.Get() });

					MakeWindow(data);
				}

				catch (const SDR::Error::Exception& error)
				{
					fail = true;
				}

				Synchro::Ptr->WindowCreated.Set();

				if (!fail)
				{
					MessageLoop();
				}
			});

			Synchro::Ptr->MainReady.Set();

			try
			{
				SDR::IPC::WaitForOne({ Synchro::Ptr->WindowCreated.Get() });
			}

			catch (const SDR::Error::Exception& error)
			{

			}
		}
	}
}

extern "C"
{
	__declspec(dllexport) void __cdecl SDR_Query(SDR::Extension::QueryData& query)
	{
		query.Name = "Preview Window";
		query.Namespace = "PreviewWindow";
		query.Author = "crashfort";
		query.Contact = "https://github.com/crashfort/";

		query.Version = 4;
	}

	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::Extension::InitializeData& data)
	{
		SDR::Error::SetPrintFormat("PreviewWindow: %s\n");
		SDR::Extension::RedirectLogOutputs(data);
	}

	__declspec(dllexport) void __cdecl SDR_StartMovie(const SDR::Extension::StartMovieData& data)
	{
		try
		{
			Synchro::Create();
		}

		catch (const SDR::Error::Exception& error)
		{
			return;
		}

		Window::Create(data);
	}

	__declspec(dllexport) void __cdecl SDR_EndMovie()
	{
		/*
			If the user didn't already close it.
		*/
		if (Window::WindowPtr)
		{
			auto handle = Window::WindowThread.native_handle();
			auto id = GetThreadId(handle);

			PostThreadMessageA(id, WM_QUIT, 0, 0);
		}
		
		if (Window::WindowThread.joinable())
		{
			Window::WindowThread.join();
		}

		Synchro::Destroy();
	}

	__declspec(dllexport) void __cdecl SDR_NewVideoFrame(const SDR::Extension::NewVideoFrameData& data)
	{
		auto lock = Synchro::Ptr->WindowPtrCS.Lock();

		if (Window::WindowPtr)
		{
			Window::WindowPtr->Update(data);
		}
	}
}
