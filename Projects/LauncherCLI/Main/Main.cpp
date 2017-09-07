#include "SDR Shared\BareWindows.hpp"
#include "SDR Shared\Error.hpp"
#include "SDR Shared\File.hpp"
#include "SDR Shared\Json.hpp"
#include "SDR Shared\IPC.hpp"
#include "SDR Plugin API\ExportTypes.hpp"
#include "rapidjson\document.h"
#include <Shlwapi.h>
#include <wrl.h>
#include <conio.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <algorithm>

namespace
{
	char LibraryName[] = R"(SDR\SourceDemoRender.dll)";
	char LibraryNameNoPrefix[] = "SourceDemoRender.dll";
	char ConfigNameNoPrefix[] = "GameConfig.json";
	char InitializeExportName[] = "SDR_Initialize";
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
	struct ServerShadowStateData : SDR::API::ShadowState
	{
		ServerShadowStateData(SDR::API::StageType stage, const char* name)
		{
			Stage = stage;
			StageName = name;

			auto pipename = SDR::API::CreatePipeName(stage);
			auto successname = SDR::API::CreateEventSuccessName(stage);
			auto failname = SDR::API::CreateEventFailureName(stage);

			Pipe.Attach(CreateNamedPipeA(pipename.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE, 1, 0, 4096, 0, nullptr));
			SDR::Error::MS::ThrowIfZero(Pipe.Get(), "Could not create inbound pipe in stage \"%s\"", StageName);

			EventSuccess.Attach(CreateEventA(nullptr, false, false, successname.c_str()));
			SDR::Error::MS::ThrowIfZero(EventSuccess.Get(), "Could not create success loader event in stage \"%s\"", StageName);

			EventFailure.Attach(CreateEventA(nullptr, false, false, failname.c_str()));
			SDR::Error::MS::ThrowIfZero(EventFailure.Get(), "Could not create failure loader event in stage \"%s\"", StageName);
		}

		HANDLE WaitEvents(HANDLE process)
		{
			printf_s("Waiting for stage \"%s\"\n", StageName);

			auto target = SDR::IPC::WaitForOne({ process, EventSuccess.Get(), EventFailure.Get() });

			ReadPipe();

			if (target == process)
			{
				SDR::Error::Make("Process exited at stage \"%s\"", StageName);
			}

			else if (target == EventSuccess.Get())
			{
				printf_s("Passed stage \"%s\"\n", StageName);
			}

			else if (target == EventFailure.Get())
			{
				TerminateProcess(process, 0);
				SDR::Error::Make("Could not pass stage \"%s\"", StageName);
			}

			return target;
		}

		void ReadPipe()
		{
			char buf[4096];
			DWORD size = sizeof(buf);
			DWORD avail = 0;

			while (true)
			{
				auto res = PeekNamedPipe(Pipe.Get(), nullptr, 0, nullptr, &avail, nullptr);

				if (res && avail > 0)
				{
					while (avail > 0)
					{
						std::memset(buf, 0, size);

						DWORD read = 0;

						auto min = std::min(size - 1, avail);
						res = ReadFile(Pipe.Get(), buf, min, &read, nullptr);

						buf[min] = 0;

						if (res)
						{
							printf_s(buf);
						}

						avail -= min;
					}
				}

				else
				{
					break;
				}
			}
		}

		const char* StageName;
		SDR::API::StageType Stage;
	};

	struct InterProcessData
	{
		decltype(LoadLibraryA)* LoadLibraryAddr;
		decltype(GetProcAddress)* GetProcAddressAddr;
		
		const char* LibraryNameAddr;
		const char* ExportNameAddr;
		const char* GamePathAddr;
		const char* GameNameAddr;
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
		auto path = data->GamePathAddr;
		auto game = data->GameNameAddr;

		auto module = data->LoadLibraryAddr(library);
		auto func = (SDR::API::SDR_Initialize)data->GetProcAddressAddr(module, loadexport);

		func(path, game);
	}

	void InjectProcess(HANDLE process, HANDLE thread, const std::string& path, const std::string& game)
	{
		printf_s("Injecting into \"%s\"\n", game.c_str());

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
			0x53,
			0x8b, 0x5d, 0x08,
			0x56,
			0x57,
			0x8b, 0x43, 0x10,
			0xff, 0x73, 0x08,
			0x8b, 0x73, 0x0c,
			0x8b, 0x7b, 0x14,
			0x89, 0x45, 0x08,
			0x8b, 0x03,
			0xff, 0xd0,
			0x56,
			0x50,
			0x8b, 0x43, 0x04,
			0xff, 0xd0,
			0x57,
			0xff, 0x75, 0x08,
			0xff, 0xd0,
			0x83, 0xc4, 0x08,
			0x5f,
			0x5e,
			0x5b,
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
		data.LibraryNameAddr = writer.PushString(LibraryName);
		data.ExportNameAddr = writer.PushString(InitializeExportName);
		data.GamePathAddr = writer.PushString(path);
		data.GameNameAddr = writer.PushString(game);

		auto dataaddr = writer.PushMemory(data);

		/*
			Enqueue the function to run on ResumeThread with parameter of InterProcessData.
		*/
		SDR::Error::MS::ThrowIfZero
		(
			QueueUserAPC((PAPCFUNC)funcaddr, thread, (ULONG_PTR)dataaddr),
			"Could not queue APC for process"
		);

		ServerShadowStateData initstage(SDR::API::StageType::Initialize, "Initialize");

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

	PROCESS_INFORMATION StartProcess(const std::string& dir, const std::string& exepath, const std::string& game, const std::string& params)
	{
		printf_s("Starting \"%s\"\n", game.c_str());

		/*
			Parameter should not have a trailing backslash as resources will fail to load.
		*/
		auto gameparamstr = dir;
		gameparamstr.pop_back();

		char args[8192];
		strcpy_s(args, exepath.c_str());
		strcat_s(args, " ");
		strcat_s(args, "-game \"");
		strcat_s(args, gameparamstr.c_str());
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

	void VerifyGameName(const std::string& name)
	{
		printf_s("Searching game config for matching name\n");

		rapidjson::Document document;

		try
		{
			document = SDR::Json::FromFile(ConfigNameNoPrefix);
		}

		catch (SDR::File::ScopedFile::ExceptionType status)
		{
			SDR::Error::Make("Could not find game config"s);
		}

		for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it)
		{
			auto gamename = it->name.GetString();
			
			if (gamename == name)
			{
				printf_s("Found \"%s\" in game config\n", gamename);
				return;
			}
		}

		SDR::Error::Make("Game not found in game config"s);
	}

	void MainProcedure(int argc, char* argv[])
	{
		if (argc < 1)
		{
			printf_s("Arguments: <exe path> <startup params ...>\n");
			return;
		}

		std::string exepath = argv[0];

		argc -= 1;
		argv += 1;

		if (PathFileExistsA(exepath.c_str()) == 0)
		{
			SDR::Error::Make("Specified path at argument 0 does not exist\n"s);
		}

		if (PathMatchSpecA(exepath.c_str(), "*.exe") == 0)
		{
			SDR::Error::Make("Specified path at argument 0 not an executable\n"s);
		}

		char curdir[SDR::File::NameSize];
		SDR::Error::MS::ThrowIfZero(GetCurrentDirectoryA(sizeof(curdir), curdir), "Could not get current directory");

		RemoveFileName(curdir);

		std::string game = PathFindFileNameA(curdir);
		game.pop_back();

		VerifyGameName(game);

		auto params = "-steam -insecure +sv_lan 1 -console"s;
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

		ServerShadowStateData loadstage(SDR::API::StageType::Load, "Load");

		auto info = StartProcess(dir, exepath, game, params);

		ScopedHandle process(info.hProcess);
		ScopedHandle thread(info.hThread);

		InjectProcess(process.Get(), thread.Get(), dir, game);

		/*
			Wait until the end of SDR::Plugin::Load() and then read back all messages.
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

		data.LibraryNameAddr = LibraryName;
		data.ExportNameAddr = InitializeExportName;
		data.GamePathAddr = "i dont know";
		data.GameNameAddr = "i dont know";

		QueueUserAPC(ProcessAPC, GetCurrentThread(), (ULONG_PTR)&data);

		SleepEx(0, 1);

		return;
	}

	void ShowLibraryVersion()
	{
		auto library = LoadLibraryA(LibraryNameNoPrefix);

		if (!library)
		{
			SDR::Error::Make("Could not load SDR library for version display");
		}

		auto func = (SDR::API::SDR_LibraryVersion)GetProcAddress(library, "SDR_LibraryVersion");
		auto version = func();

		printf_s("SDR library version: %d\n", version);

		FreeLibrary(library);
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
