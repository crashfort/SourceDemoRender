#include "SDR Shared\BareWindows.hpp"
#include "SDR Shared\Error.hpp"
#include "SDR Plugin API\ExportTypes.hpp"
#include "SDR Shared\File.hpp"
#include "rapidjson\document.h"
#include <Shlwapi.h>
#include <wrl.h>
#include <conio.h>
#include <cstdio>
#include <cstdint>
#include <string>

namespace
{
	char LibraryName[] = R"(SDR\SourceDemoRender.dll)";
	char LibraryNameNoPrefix[] = "SourceDemoRender.dll";
	char ConfigNameNoPrefix[] = "GameConfig.json";
	char InitializeExportName[] = "SDR_Initialize";
	char EventName[] = "SDR_LAUNCHER";
}

namespace
{
	struct ProcessWriter
	{
		ProcessWriter(HANDLE process, void* startaddress) : Process(process), Address((uint8_t*)(startaddress))
		{

		}

		uint8_t* PushMemory(const void* address, size_t size)
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

		uint8_t* PushString(const std::string& string)
		{
			return PushMemory(string.c_str(), string.size() + 1);
		}

		template <size_t Size>
		uint8_t* PushString(char(&buffer)[Size])
		{
			return PushMemory(buffer, sizeof(buffer));
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
	struct InterProcessData
	{
		decltype(LoadLibraryA)* LoadLibraryAddr;
		decltype(GetProcAddress)* GetProcAddressAddr;
		decltype(OpenEventA)* OpenEventAddr;
		decltype(SetEvent)* SetEventAddr;
		decltype(CloseHandle)* CloseHandleAddr;
		
		void* LibraryNameAddr;
		void* ExportNameAddr;
		void* EventNameAddr;
		void* GamePathAddr;
		void* GameNameAddr;

		SDR::API::InitializeCode Code;
	};

	/*
		This is the code that will run in the other process.
		No errors are checked because it would leave the process in an intermediate state.
		On any error the process will automatically crash.
	*/
	VOID CALLBACK ProcessAPC(ULONG_PTR param)
	{
		auto data = (InterProcessData*)param;

		auto library = (const char*)data->LibraryNameAddr;
		auto loadexport = (const char*)data->ExportNameAddr;
		auto path = (const char*)data->GamePathAddr;
		auto game = (const char*)data->GameNameAddr;
		auto eventname = (const char*)data->EventNameAddr;

		auto module = data->LoadLibraryAddr(library);
		auto func = (SDR::API::SDR_Initialize)data->GetProcAddressAddr(module, loadexport);

		data->Code = func(path, game);

		auto event = data->OpenEventAddr(EVENT_MODIFY_STATE, false, eventname);

		/*
			It's now safe to read the status code.
		*/
		data->SetEventAddr(event);
		data->CloseHandleAddr(event);
	}

	void InjectProcess(HANDLE process, HANDLE thread, const std::string& path, const std::string& game)
	{
		struct FailTerminateData
		{
			FailTerminateData(HANDLE process) : Process(process)
			{

			}

			~FailTerminateData()
			{
				if (Fail)
				{
					TerminateProcess(Process, 0);
				}
			}

			HANDLE Process;
			bool Fail = true;
		};

		/*
			Ensure the process is closed on any error.
		*/
		FailTerminateData terminator(process);

		printf_s("Injecting into \"%s\"\n", game.c_str());

		VirtualMemory memory(process, 1024);
		ProcessWriter writer(process, memory.Address);

		/*
			Produced from ProcessAPC above in Release with machine code listing output.
			Any change to ProcessAPC or InterProcessData will need a simulation rebuild in Release.
		*/
		uint8_t function[] =
		{
			0x55,
			0x8b, 0xec,
			0x51,
			0x8b, 0x4d, 0x08,
			0x53,
			0x56,
			0x57,
			0x8b, 0x41, 0x1c,
			0xff, 0x71, 0x14,
			0x8b, 0x71, 0x18,
			0x8b, 0x59, 0x20,
			0x8b, 0x79, 0x24,
			0x89, 0x45, 0xfc,
			0x8b, 0x01,
			0xff, 0xd0,
			0x56,
			0x50,
			0x8b, 0x45, 0x08,
			0x8b, 0x40, 0x04,
			0xff, 0xd0,
			0x57,
			0x53,
			0xff, 0xd0,
			0x8b, 0x5d, 0x08,
			0x83, 0xc4, 0x08,
			0xff, 0x75, 0xfc,
			0x89, 0x43, 0x28,
			0x8b, 0x43, 0x08,
			0x6a, 0x00,
			0x6a, 0x02,
			0xff, 0xd0,
			0x8b, 0x4b, 0x0c,
			0x8b, 0xf0,
			0x56,
			0xff, 0xd1,
			0x8b, 0x43, 0x10,
			0x56,
			0xff, 0xd0,
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
		auto funcaddr = writer.PushMemory(function, sizeof(function));

		InterProcessData data;
		
		/*
			These kernel32.dll functions will always be in the same place between processes.
		*/
		data.LoadLibraryAddr = LoadLibraryA;
		data.GetProcAddressAddr = GetProcAddress;
		data.OpenEventAddr = OpenEventA;
		data.SetEventAddr = SetEvent;
		data.CloseHandleAddr = CloseHandle;

		/*
			All referenced strings must be allocated in the other process too.
		*/
		data.LibraryNameAddr = writer.PushString(LibraryName);
		data.ExportNameAddr = writer.PushString(InitializeExportName);
		data.GamePathAddr = writer.PushString(path);
		data.GameNameAddr = writer.PushString(game);
		data.EventNameAddr = writer.PushString(EventName);
		
		data.Code = SDR::API::InitializeCode::GeneralFailure;

		auto dataaddr = writer.PushMemory(&data, sizeof(data));

		/*
			Enqueue the function to run on ResumeThread with parameter of InterProcessData.
		*/
		SDR::Error::MS::ThrowIfZero
		(
			QueueUserAPC((PAPCFUNC)funcaddr, thread, (ULONG_PTR)dataaddr),
			"Could not queue APC for process"
		);

		/*
			Event that will notify the completion of the initialize routine.
		*/
		auto event = CreateEventA(nullptr, false, false, EventName);
		
		SDR::Error::MS::ThrowIfZero(event, "Could not create launcher sync event");

		/*
			Our initialize function will now run inside the other process.
		*/
		ResumeThread(thread);

		auto handles =
		{
			event,
			process
		};

		/*
			Wait for the signal that it's safe to read back the status code, or if there
			was an error and the process ended.
		*/
		auto waitres = WaitForMultipleObjects(handles.size(), handles.begin(), false, INFINITE);

		if (waitres == WAIT_FAILED)
		{
			SDR::Error::MS::ThrowLastError("Could not wait for launcher event");
		}

		auto maxwait = WAIT_OBJECT_0 + handles.size();

		if (waitres < maxwait)
		{
			auto index = waitres - WAIT_OBJECT_0;

			/*
				Event handle.
			*/
			if (index == 0)
			{
				printf_s("Received remote SDR signal\n");
			}
			
			/*
				Process handle.
			*/
			else  if (index == 1)
			{
				SDR::Error::Make("Process exited");
			}
		}

		/*
			Now read back the status code.
		*/
		SDR::Error::MS::ThrowIfZero
		(
			ReadProcessMemory(process, dataaddr, &data, sizeof(data), nullptr),
			"Could not read process memory for status code"
		);

		switch (data.Code)
		{
			case SDR::API::InitializeCode::GeneralFailure:
			{
				printf_s("Could not remotely initialize SDR\n");
				break;
			}

			case SDR::API::InitializeCode::Success:
			{
				printf_s("SDR initialized in \"%s\"\n", game.c_str());
				terminator.Fail = false;
				break;
			}

			case SDR::API::InitializeCode::CouldNotInitializeHooks:
			{
				printf_s("Could not initialize hooks inside SDR\n");
				break;
			}

			case SDR::API::InitializeCode::CouldNotCreateLibraryIntercepts:
			{
				printf_s("Could not create library intercepts inside SDR\n");
				break;
			}

			case SDR::API::InitializeCode::CouldNotEnableLibraryIntercepts:
			{
				printf_s("Could not enable library intercepts inside SDR\n");
				break;
			}
		}
	}

	template <size_t Size>
	void RemoveFileName(char(&buffer)[Size])
	{
		PathRemoveFileSpecA(buffer);
		strcat_s(buffer, "\\");
	}

	PROCESS_INFORMATION StartProcess(const std::string& dir, const std::string& exepath, const std::string& game, const std::string& params)
	{
		printf_s("Starting \"%s\"\n", game.c_str());

		char args[1024];
		strcpy_s(args, exepath.c_str());
		strcat_s(args, " ");
		strcat_s(args, "-game ");
		strcat_s(args, game.c_str());
		strcat_s(args, " ");
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

	void VerifyGameName(const std::string& name)
	{
		SDR::File::ScopedFile config;

		try
		{
			config.Assign(ConfigNameNoPrefix, "rb");
		}

		catch (SDR::File::ScopedFile::ExceptionType status)
		{
			SDR::Error::Make("Could not find game config");
		}

		printf_s("Searching game config for matching name\n");

		auto data = config.ReadAll();
		std::string strdata((const char*)data.data(), data.size());

		rapidjson::Document document;
		document.Parse(strdata.c_str());

		for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it)
		{
			auto gamename = it->name.GetString();
			
			if (gamename == name)
			{
				printf_s("Found \"%s\" in game config\n", gamename);
				return;
			}
		}

		SDR::Error::Make("Game not found in game config");
	}

	void MainProcedure(int argc, char* argv[])
	{
		if (argc < 1)
		{
			printf_s("Arguments: <exe path> <startup params ...>\n");
			return;
		}

		char curdir[1024];
		SDR::Error::MS::ThrowIfZero(GetCurrentDirectoryA(sizeof(curdir), curdir), "Could not get current directory");

		RemoveFileName(curdir);

		std::string game = PathFindFileNameA(curdir);
		game.pop_back();

		std::string exepath = argv[0];

		argc -= 1;
		argv += 1;

		if (PathFileExistsA(exepath.c_str()) == 0)
		{
			SDR::Error::Make("Specified path at argument 0 does not exist\n");
		}

		if (PathMatchSpecA(exepath.c_str(), "*.exe") == 0)
		{
			SDR::Error::Make("Specified path at argument 0 not an executable\n");
		}

		VerifyGameName(game);

		std::string params = "-steam -insecure +sv_lan 1 -console";
		printf_s("Appending parameters: \"%s\"\n", params.c_str());
		
		std::string dir = curdir;

		printf_s("Game: \"%s\"\n", game.c_str());
		printf_s("Executable path: \"%s\"\n", exepath.c_str());
		printf_s("Directory: \"%s\"\n", dir.c_str());

		for (size_t i = 0; i < argc; i++)
		{
			params += ' ';
			params += argv[i];
		}

		printf_s("Parameters: \"%s\"\n", params.c_str());

		auto info = StartProcess(dir, exepath, game, params);

		ScopedHandle process(info.hProcess);
		ScopedHandle thread(info.hThread);

		InjectProcess(process.Get(), thread.Get(), dir, game);
	}

	void ShowLibraryVersion()
	{
		auto module = LoadLibraryA(LibraryNameNoPrefix);

		if (!module)
		{
			printf_s("Could not load SDR library for version display\n");
		}

		else
		{
			auto func = (SDR::API::SDR_LibraryVersion)GetProcAddress(module, "SDR_LibraryVersion");

			if (!func)
			{
				printf_s("Could not get SDR library version export function\n");
			}

			else
			{
				auto version = func();
				printf_s("SDR library version: %d\n", version);
			}
		}

		FreeLibrary(module);
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
		data.OpenEventAddr = OpenEventA;
		data.SetEventAddr = SetEvent;
		data.CloseHandleAddr = CloseHandle;

		data.LibraryNameAddr = LibraryName;
		data.ExportNameAddr = InitializeExportName;
		data.GamePathAddr = "i dont know";
		data.GameNameAddr = "i dont know";
		data.EventNameAddr = EventName;

		QueueUserAPC(ProcessAPC, GetCurrentThread(), (ULONG_PTR)&data);

		SleepEx(0, 1);
	}
}

void main(int argc, char* argv[])
{
	try
	{
		EnsureFileIsPresent(LibraryNameNoPrefix);
		EnsureFileIsPresent(ConfigNameNoPrefix);

		ShowLibraryVersion();

		/*
			Don't need our own name.
		*/
		argv++;
		argc--;

		MainProcedure(argc, argv);
	}

	catch (const SDR::Error::Exception& error)
	{
		
	}

	printf_s("You can close this window now\n");

	_getch();
}
