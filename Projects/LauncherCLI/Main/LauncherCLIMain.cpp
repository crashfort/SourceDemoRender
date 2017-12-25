#include <SDR Shared\BareWindows.hpp>
#include <SDR Shared\Error.hpp>
#include <SDR Shared\File.hpp>
#include <SDR Shared\Json.hpp>
#include <SDR Shared\IPC.hpp>
#include <SDR Library API\ExportTypes.hpp>
#include <SDR LauncherCLI API\LauncherCLIAPI.hpp>
#include <rapidjson\document.h>
#include <Shlwapi.h>
#include <wrl.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <algorithm>
#include <thread>
#include <functional>

#include <Richedit.h>
#include <CommCtrl.h>

using namespace std::literals;

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
					SDR::Error::MS::ThrowLastError("Could not create event \"MainReady\"");
				}

				if (!WindowCreated.Event.IsValid())
				{
					SDR::Error::MS::ThrowLastError("Could not create event \"WindowCreated\"");
				}
			}

			EventData MainReady;
			EventData WindowCreated;

			Microsoft::WRL::Wrappers::CriticalSection WindowCS;
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
		HWND CreateRichEdit(HWND owner, int x, int y, int width, int height, HINSTANCE instance)
		{
			auto library = LoadLibraryA("Msftedit.dll");

			if (!library)
			{
				SDR::Error::MS::ThrowLastError("Could not load rich edit control library");
			}

			auto classname = MSFTEDIT_CLASS;
			auto text = L"";
			auto style = ES_READONLY | ES_AUTOVSCROLL | ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_TABSTOP;

			auto hwnd = CreateWindowExW(0, classname, text, style, x, y, width, height, owner, nullptr, instance, nullptr);
			return hwnd;
		}

		HWND WindowHandle = nullptr;
		HWND TextControl = nullptr;
		std::thread Thread;

		CHARFORMAT2A GetDefaultFormat()
		{
			CHARFORMAT2A format = {};
			format.cbSize = sizeof(format);
			format.dwMask |= CFM_COLOR | CFM_FACE | CFM_SIZE;
			strcpy_s(format.szFaceName, "Consolas");
			format.crTextColor = SDR::LauncherCLI::Colors::White;
			
			/*
				10px
			*/
			format.yHeight = 10 * 20;

			return format;
		}

		struct TextFormatData
		{
			COLORREF Color;
			std::string Text;
		};

		std::vector<TextFormatData> FormatText(const char* text)
		{
			struct TokenData
			{
				static TokenData Make(const char* name, COLORREF color)
				{
					TokenData ret;
					ret.Start = SDR::String::Format("{%s}", name);
					ret.End = SDR::String::Format("{/%s}", name);
					ret.Color = color;

					return ret;
				}

				std::string Start;
				std::string End;

				COLORREF Color;
			};

			auto tokens =
			{
				TokenData::Make("dark", SDR::LauncherCLI::Colors::Dark),
				TokenData::Make("red", SDR::LauncherCLI::Colors::Red),
				TokenData::Make("green", SDR::LauncherCLI::Colors::Green),
				TokenData::Make("string", SDR::LauncherCLI::Colors::String),
				TokenData::Make("number", SDR::LauncherCLI::Colors::Number),
			};

			std::vector<TextFormatData> parts;

			TextFormatData* current = nullptr;

			auto ptr = text;

			for (; *ptr;)
			{
				if (*ptr != '{')
				{
					if (!current)
					{
						parts.emplace_back();
						current = &parts.back();
						
						current->Color = SDR::LauncherCLI::Colors::White;
					}
					
					current->Text.push_back(*ptr);
					
					++ptr;
					continue;
				}

				for (const auto& token : tokens)
				{
					if (SDR::String::StartsWith(ptr, token.Start.c_str()))
					{
						parts.emplace_back();
						current = &parts.back();

						current->Color = token.Color;

						ptr += token.Start.size();

						while (true)
						{
							if (SDR::String::StartsWith(ptr, token.End.c_str()))
							{
								ptr += token.End.size();
								break;
							}

							current->Text.push_back(*ptr);
							++ptr;
						}

						current = nullptr;
						break;
					}
				}
			}

			return parts;
		}

		void AppendLogText(const char* text)
		{
			if (!WindowHandle)
			{
				return;
			}

			CHARRANGE range;
			range.cpMin = -1;
			range.cpMax = -1;

			auto charformat = GetDefaultFormat();

			auto textformats = FormatText(text);

			for (const auto& entry : textformats)
			{
				charformat.crTextColor = entry.Color;
				SendMessageA(TextControl, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&charformat);

				SendMessageA(TextControl, EM_EXSETSEL, 0, (LPARAM)&range);
				SendMessageA(TextControl, EM_REPLACESEL, 0, (LPARAM)entry.Text.c_str());
			}

			SendMessageA(TextControl, WM_VSCROLL, SB_BOTTOM, 0);
		}

		LRESULT CALLBACK TextControlProcedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR subid, DWORD_PTR ref)
		{
			switch (message)
			{
				case WM_MOUSEWHEEL:
				{
					if (GET_WHEEL_DELTA_WPARAM(wparam) > 0)
					{
						SendMessageA(hwnd, WM_VSCROLL, SB_LINEUP, 0);
						SendMessageA(hwnd, WM_VSCROLL, SB_LINEUP, 0);
						SendMessageA(hwnd, WM_VSCROLL, SB_LINEUP, 0);
					}

					else if (GET_WHEEL_DELTA_WPARAM(wparam) < 0)
					{
						SendMessageA(hwnd, WM_VSCROLL, SB_LINEDOWN, 0);
						SendMessageA(hwnd, WM_VSCROLL, SB_LINEDOWN, 0);
						SendMessageA(hwnd, WM_VSCROLL, SB_LINEDOWN, 0);
					}

					return 1;
				}
			}

			if (message == WM_KEYDOWN)
			{
				SendMessageA(WindowHandle, message, wparam, lparam);
			}

			return DefSubclassProc(hwnd, message, wparam, lparam);
		}

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
					int width = LOWORD(lparam);
					int height = HIWORD(lparam);

					auto flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
					SetWindowPos(TextControl, nullptr, 0, 0, width, height, flags);

					return 0;
				}

				case WM_COPYDATA:
				{
					auto copydata = (COPYDATASTRUCT*)lparam;
					auto data = (SDR::LauncherCLI::AddMessageData*)copydata->lpData;

					AppendLogText(data->Text);
					return 1;
				}

				case WM_CONTEXTMENU:
				{
					int a = 5;
					break;
				}

				case WM_KEYDOWN:
				{
					if (wparam == VK_RETURN)
					{
						PostQuitMessage(0);
						return 0;
					}

					break;
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

			WindowHandle = nullptr;
			TextControl = nullptr;
		}

		void MakeWindow(HINSTANCE instance)
		{
			{
				auto classname = "SDR_LAUNCHERCLI_OWNER_CLASS";

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
						SDR::Error::MS::ThrowLastError("Could not register owner window class");
					}
				}

				RECT rect = {};
				rect.right = 960;
				rect.bottom = 640;

				const auto style = WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX | WS_CLIPCHILDREN;

				AdjustWindowRect(&rect, style, false);

				auto title = "SDR Launcher CLI";
				auto posx = CW_USEDEFAULT;
				auto posy = CW_USEDEFAULT;
				auto width = rect.right - rect.left;
				auto height = rect.bottom - rect.top;

				WindowHandle = CreateWindowExA(0, classname, title, style, posx, posy, width, height, nullptr, nullptr, instance, nullptr);

				if (!WindowHandle)
				{
					SDR::Error::MS::ThrowLastError("Could not create window");
				}

				TextControl = CreateRichEdit(WindowHandle, 0, 0, width, height, instance);

				if (!TextControl)
				{
					SDR::Error::MS::ThrowLastError("Could not create rich edit control");
				}

				SetWindowSubclass(TextControl, TextControlProcedure, 0, 0);

				SendMessageA(TextControl, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
				SendMessageA(TextControl, EM_SHOWSCROLLBAR, SB_VERT, 1);

				auto format = GetDefaultFormat();
				SendMessageA(TextControl, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
			}

			ShowWindow(WindowHandle, SW_SHOW);
		}

		void Create(HINSTANCE instance)
		{
			Thread = std::thread([=]()
			{
				bool fail = false;

				try
				{
					SDR::IPC::WaitForOne({ Synchro::Ptr->MainReady.Get() });

					MakeWindow(instance);
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

namespace
{
	namespace Local
	{
		template <typename... Args>
		void Message(const char* format, Args&&... args)
		{
			auto text = SDR::String::Format(format, std::forward<Args>(args)...);
			Window::AppendLogText(text.c_str());
		}

		template <typename... Args>
		void Warning(const char* format, Args&&... args)
		{
			auto text = SDR::String::Format(format, std::forward<Args>(args)...);
			Window::AppendLogText(text.c_str());
		}

		char LibraryName[] = "SourceDemoRender.dll";
		char GameConfigName[] = "GameConfig.json";
		char InitializeExportName[] = "SDR_Initialize";
	}
}

namespace
{
	struct ProcessWriter
	{
		ProcessWriter(HANDLE process, void* startaddress) : Process(process), Address((uint8_t*)(startaddress))
		{

		}

		void* PushMemory(const void* address, size_t size)
		{
			SIZE_T written;

			SDR::Error::MS::ThrowIfZero
			(
				WriteProcessMemory(Process, Address, address, size, &written),
				"Could not write process memory"
			);

			auto retaddr = Address;

			Address += written;

			return retaddr;
		}

		template <typename T>
		auto PushMemory(const T& type)
		{
			return PushMemory(&type, sizeof(T));
		}

		auto PushString(const std::string& string)
		{
			return (const char*)PushMemory(string.c_str(), string.size() + 1);
		}

		template <size_t Size>
		auto PushString(char(&buffer)[Size])
		{
			return (const char*)PushMemory(buffer, sizeof(buffer));
		}

		HANDLE Process;
		uint8_t* Address;
	};

	struct VirtualMemory
	{
		VirtualMemory(HANDLE process, size_t size, DWORD flags = MEM_COMMIT | MEM_RESERVE, DWORD protect = PAGE_EXECUTE_READWRITE) : Process(process)
		{
			Address = VirtualAllocEx(process, nullptr, size, flags, protect);
			SDR::Error::ThrowIfNull(Address, "Could not allocate virtual memory");
		}

		~VirtualMemory()
		{
			VirtualFreeEx(Process, Address, 0, MEM_RELEASE);
		}
			
		void* Address = nullptr;
		HANDLE Process;
	};

	using ScopedHandle = Microsoft::WRL::Wrappers::HandleT
	<
		Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits
	>;
}

namespace
{
	struct ServerShadowStateData : SDR::LauncherCLI::Load::ShadowState
	{
		ServerShadowStateData(SDR::LauncherCLI::Load::StageType stage, const char* name)
		{
			StageName = name;

			auto successname = SDR::LauncherCLI::Load::CreateEventSuccessName(stage);
			auto failname = SDR::LauncherCLI::Load::CreateEventFailureName(stage);

			EventSuccess.Attach(CreateEventA(nullptr, false, false, successname.c_str()));
			SDR::Error::MS::ThrowIfZero(EventSuccess.Get(), "Could not create success event in stage \"%s\"", StageName);

			EventFailure.Attach(CreateEventA(nullptr, false, false, failname.c_str()));
			SDR::Error::MS::ThrowIfZero(EventFailure.Get(), "Could not create failure event in stage \"%s\"", StageName);
		}

		void WaitEvents(HANDLE process)
		{
			Local::Message("Waiting for stage: {string}\"%s\"{/string}\n", StageName);

			auto target = SDR::IPC::WaitForOne({ process, EventSuccess.Get(), EventFailure.Get() });

			if (target == process)
			{
				SDR::Error::Make("Process exited at stage \"%s\"", StageName);
			}

			else if (target == EventSuccess.Get())
			{
				Local::Message("Passed stage: {string}\"%s\"{/string}\n", StageName);
			}

			else if (target == EventFailure.Get())
			{
				TerminateProcess(process, 0);
				SDR::Error::Make("Could not pass stage \"%s\"", StageName);
			}
		}

		const char* StageName;
	};

	struct InterProcessData
	{
		decltype(LoadLibraryA)* LoadLibraryAddr;
		decltype(GetProcAddress)* GetProcAddressAddr;
		
		const char* LibraryNameAddr;
		const char* ExportNameAddr;
		const char* ResourcePathAddr;
		const char* GamePathAddr;

		HWND LauncherCLI;
	};

	/*
		This is the code that will run in the other process.
		No errors are checked because it would leave the process in an intermediate state.
		On any error the process will automatically crash.
	*/
	VOID CALLBACK ProcessAPC(ULONG_PTR param)
	{
		auto data = (InterProcessData*)param;

		auto library = data->LibraryNameAddr;
		auto loadexport = data->ExportNameAddr;
		auto respath = data->ResourcePathAddr;
		auto gamepath = data->GamePathAddr;
		auto hwnd = data->LauncherCLI;

		auto module = data->LoadLibraryAddr(library);
		auto func = (SDR::API::SDR_Initialize)data->GetProcAddressAddr(module, loadexport);

		SDR::API::InitializeData initdata;
		initdata.ResourcePath = respath;
		initdata.GamePath = gamepath;
		initdata.LauncherCLI = hwnd;

		func(initdata);
	}

	void InjectProcess(HANDLE process, HANDLE thread, const std::string& resourcepath, const std::string& gamepath)
	{
		VirtualMemory memory(process, 4096);
		ProcessWriter writer(process, memory.Address);

		/*
			Produced from ProcessAPC above in Release with machine code listing output.
			Any change to ProcessAPC or InterProcessData will need a simulation rebuild in Release.
		*/		
		uint8_t function[] =
		{
			0x55,
			0x8b, 0xec,
			0x83, 0xec, 0x10,
			0x53,
			0x56,
			0x57,
			0x8b, 0x7d, 0x08,
			0x8b, 0x47, 0x14,
			0xff, 0x77, 0x08,
			0x8b, 0x77, 0x0c,
			0x8b, 0x5f, 0x10,
			0x89, 0x45, 0x08,
			0x8b, 0x47, 0x18,
			0x89, 0x45, 0xfc,
			0x8b, 0x07,
			0xff, 0xd0,
			0x56,
			0x50,
			0x8b, 0x47, 0x04,
			0xff, 0xd0,
			0x8b, 0x4d, 0x08,
			0x89, 0x4d, 0xf4,
			0x8b, 0x4d, 0xfc,
			0x89, 0x4d, 0xf8,
			0x8d, 0x4d, 0xf0,
			0x51,
			0x89, 0x5d, 0xf0,
			0xff, 0xd0,
			0x83, 0xc4, 0x04,
			0x5f,
			0x5e,
			0x5b,
			0x8b, 0xe5,
			0x5d,
			0xc2, 0x04, 0x00,
		};

		/*
			Memory location in other process.
		*/
		auto funcaddr = writer.PushMemory(function);

		InterProcessData data;
		
		/*
			These kernel32.dll functions will always be in the same place between processes.
		*/
		data.LoadLibraryAddr = LoadLibraryA;
		data.GetProcAddressAddr = GetProcAddress;

		/*
			All referenced strings must be allocated in the other process too.
		*/
		data.LibraryNameAddr = writer.PushString(resourcepath + Local::LibraryName);
		data.ExportNameAddr = writer.PushString(Local::InitializeExportName);
		data.ResourcePathAddr = writer.PushString(resourcepath);
		data.GamePathAddr = writer.PushString(gamepath);

		data.LauncherCLI = Window::WindowHandle;

		auto dataaddr = writer.PushMemory(data);

		/*
			Enqueue the function to run on ResumeThread with parameter of InterProcessData.
		*/
		SDR::Error::MS::ThrowIfZero
		(
			QueueUserAPC((PAPCFUNC)funcaddr, thread, (ULONG_PTR)dataaddr),
			"Could not queue APC for process"
		);

		ServerShadowStateData initstage(SDR::LauncherCLI::Load::StageType::Initialize, "Initialize");

		/*
			Our initialize function will now run inside the other process.
		*/
		ResumeThread(thread);

		/*
			Wait for either success or failure signal or if there was an error and the process ended.
		*/
		initstage.WaitEvents(process);
	}

	template <size_t Size>
	void RemoveFileName(char(&buffer)[Size])
	{
		PathRemoveFileSpecA(buffer);
		strcat_s(buffer, "\\");
	}

	PROCESS_INFORMATION StartProcess(const std::string& dir, const std::string& exepath, const std::string& params)
	{
		char args[8192];
		strcpy_s(args, exepath.c_str());
		strcat_s(args, " ");
		strcat_s(args, "-game \"");
		strcat_s(args, dir.c_str());
		strcat_s(args, "\" ");
		strcat_s(args, params.c_str());

		STARTUPINFOA startinfo = {};
		startinfo.cb = sizeof(startinfo);

		PROCESS_INFORMATION procinfo;

		SDR::Error::MS::ThrowIfZero
		(
			CreateProcessA
			(
				exepath.c_str(),
				args,
				nullptr,
				nullptr,
				false,
				CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED | DETACHED_PROCESS,
				nullptr,
				dir.c_str(),
				&startinfo,
				&procinfo
			),
			"Could not create process"
		);

		return procinfo;
	}

	std::string GetDisplayName(const char* gamepath)
	{
		Local::Message("Searching game config for matching name\n");

		rapidjson::Document document;

		try
		{
			document = SDR::Json::FromFile(Local::GameConfigName);
		}

		catch (SDR::File::ScopedFile::ExceptionType status)
		{
			SDR::Error::Make("Could not find game config"s);
		}

		for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it)
		{
			auto gamename = it->name.GetString();
			
			if (SDR::String::EndsWith(gamepath, gamename))
			{
				gamename = SDR::Json::GetString(it->value, "DisplayName");

				Local::Message("Found {string}\"%s\"{/string} in game config\n", gamename);
				
				return gamename;
			}
		}

		SDR::Error::Make("Game not found in game config"s);
	}

	void MainProcedure(const std::string& exepath, const std::string& gamepath, const std::string& params)
	{
		if (PathFileExistsA(exepath.c_str()) == 0)
		{
			SDR::Error::Make("Specified path at /GAME does not exist"s);
		}

		if (PathMatchSpecA(exepath.c_str(), "*.exe") == 0)
		{
			SDR::Error::Make("Specified path at /GAME not an executable"s);
		}

		if (PathFileExistsA(gamepath.c_str()) == 0)
		{
			SDR::Error::Make("Specified path at /PATH does not exist"s);
		}

		if (PathIsDirectoryA(gamepath.c_str()) == 0)
		{
			SDR::Error::Make("Specified path at /PATH not a directory"s);
		}

		char curdir[SDR::File::NameSize];
		SDR::Error::MS::ThrowIfZero(GetCurrentDirectoryA(sizeof(curdir), curdir), "Could not get current directory");
		strcat_s(curdir, "\\");

		/*
			Parameter should not have a trailing backslash as resources will fail to load.
		*/
		char gamefolder[SDR::File::NameSize];
		strcpy_s(gamefolder, gamepath.c_str());

		auto displayname = GetDisplayName(gamefolder);

		Local::Message("Game: {string}\"%s\"{/string}\n", displayname.c_str());
		Local::Message("Executable: {string}\"%s\"{/string}\n", exepath.c_str());
		Local::Message("Directory: {string}\"%s\"{/string}\n", gamefolder);
		Local::Message("Resource: {string}\"%s\"{/string}\n", curdir);
		Local::Message("Parameters: {string}\"%s\"{/string}\n", params.c_str());

		ServerShadowStateData loadstage(SDR::LauncherCLI::Load::StageType::Load, "Load");

		Local::Message("Starting: {string}\"%s\"{/string}\n", displayname.c_str());

		auto info = StartProcess(gamefolder, exepath, params);

		ScopedHandle process(info.hProcess);
		ScopedHandle thread(info.hThread);

		Local::Message("Injecting into: {string}\"%s\"{/string}\n", displayname.c_str());

		InjectProcess(process.Get(), thread.Get(), curdir, gamefolder);

		/*
			Wait until the end of SDR::Library::Load() and then read back all messages.
		*/
		loadstage.WaitEvents(process.Get());
	}

	void EnsureFileIsPresent(const char* name)
	{
		if (PathFileExistsA(name) == 0)
		{
			SDR::Error::Make("Required file \"%s\" does not exist", name);
		}
	}

	void SimulateMachineCode()
	{
		InterProcessData data;
		data.LoadLibraryAddr = LoadLibraryA;
		data.GetProcAddressAddr = GetProcAddress;

		data.LibraryNameAddr = Local::LibraryName;
		data.ExportNameAddr = Local::InitializeExportName;
		data.ResourcePathAddr = "i dont know";
		data.GamePathAddr = "i dont know";

		data.LauncherCLI = nullptr;

		QueueUserAPC(ProcessAPC, GetCurrentThread(), (ULONG_PTR)&data);

		SleepEx(0, 1);

		return;
	}

	void ShowLibraryVersion()
	{
		/*
			Safe because nothing external is referenced.
		*/
		auto library = LoadLibraryExA(Local::LibraryName, nullptr, DONT_RESOLVE_DLL_REFERENCES);

		if (!library)
		{
			SDR::Error::Make("Could not load SDR library for version display");
		}

		auto func = (SDR::API::SDR_LibraryVersion)GetProcAddress(library, "SDR_LibraryVersion");
		auto version = func();

		Local::Message("SDR library version: {number}%d{/number}\n", version);

		FreeLibrary(library);
	}
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int showcommand)
{
	SDR::Log::SetWarningFunction([](std::string&& text)
	{
		Local::Warning(text.c_str());
	});

	try
	{
		Synchro::Create();

		Window::Create(instance);

		auto argv = *__p___argv();
		auto argc = *__p___argc();

		/*
			Don't need our own name.
		*/
		argv++;
		argc--;

		ShowLibraryVersion();

		/*
			/GAME "" /PARAMS "" /
		*/
		if (argc < 1)
		{
			SDR::Error::Make("Arguments: /GAME \"<exe path>\" /PATH \"<game path>\" /PARAMS \"<startup params>\"");
		}

		std::string exepath;
		std::string gamepath;
		std::string params = "-steam -insecure +sv_lan 1 -console"s;

		Local::Message("Appending parameters: {string}\"%s\"{/string}\n", params.c_str());

		for (size_t i = 0; i < argc; i++)
		{
			if (SDR::String::IsEqual(argv[i], "/GAME"))
			{
				exepath = argv[++i];
				continue;
			}

			if (SDR::String::IsEqual(argv[i], "/PATH"))
			{
				gamepath = argv[++i];
				continue;
			}

			if (SDR::String::IsEqual(argv[i], "/PARAMS"))
			{
				params += ' ';
				params += argv[++i];
				
				continue;
			}
		}

		if (exepath.empty())
		{
			SDR::Error::Make("Required switch \"/GAME\" not found");
		}

		if (gamepath.empty())
		{
			SDR::Error::Make("Required switch \"/PATH\" not found");
		}

		EnsureFileIsPresent(Local::LibraryName);
		EnsureFileIsPresent(Local::GameConfigName);

		MainProcedure(exepath, gamepath, params);
	}

	catch (const SDR::Error::Exception& error)
	{
		
	}

	Local::Message("You can close this window now\n");

	if (Window::Thread.joinable())
	{
		Window::Thread.join();
	}

	Synchro::Destroy();

	return 0;
}
