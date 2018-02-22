#include "PrecompiledHeader.hpp"
#include "LibraryInterface.hpp"
#include "Application\Application.hpp"
#include <SDR Library API\LibraryAPI.hpp>
#include <SDR LauncherCLI API\LauncherCLIAPI.hpp>

#include "Interface\Application\Modules\Shared\Console.hpp"

extern "C"
{
	#include <libavcodec\avcodec.h>
	#include <libavformat\avformat.h>
}

namespace
{
	namespace Local
	{
		std::string ResourcePath;
		std::string GamePath;
		HWND LauncherCLI;
	}
}

namespace
{
	enum
	{
		LibraryVersion = 31,
	};
}

namespace
{
	namespace Commands
	{
		void Version()
		{
			SDR::Log::Message("SDR: Library version: %d\n", LibraryVersion);
		}

		/*
			Creation has to be delayed as the necessary console stuff isn't available earlier.
		*/
		SDR::StartupFunctionAdder A1("LibraryInterface console commands", []()
		{
			SDR::Console::MakeCommand("sdr_version", Commands::Version);
		});
	}

	void RegisterLAV()
	{
		avcodec_register_all();
		av_register_all();
	}

	struct LoadFuncData : SDR::LauncherCLI::ShadowState
	{
		void Create(SDR::LauncherCLI::StageType stage)
		{
			auto successname = SDR::LauncherCLI::CreateEventSuccessName(stage);
			auto failname = SDR::LauncherCLI::CreateEventFailureName(stage);

			EventSuccess.Attach(OpenEventA(EVENT_MODIFY_STATE, false, successname.c_str()));
			EventFailure.Attach(OpenEventA(EVENT_MODIFY_STATE, false, failname.c_str()));

			if (!EventSuccess.IsValid())
			{
				SDR::Error::Microsoft::ThrowLastError("Could not open success event");
			}

			if (!EventFailure.IsValid())
			{
				SDR::Error::Microsoft::ThrowLastError("Could not open failure event");
			}
		}

		~LoadFuncData()
		{
			if (Failure)
			{
				if (EventFailure.IsValid())
				{
					SetEvent(EventFailure.Get());
				}
			}

			else
			{
				if (EventSuccess.IsValid())
				{
					SetEvent(EventSuccess.Get());
				}
			}
		}

		bool Failure = true;
	};

	void WriteToLauncherCLI(const char* text)
	{
		SDR::LauncherCLI::AddMessageData data;
		strcpy_s(data.Text, text);

		COPYDATASTRUCT copydata = {};
		copydata.dwData = SDR::LauncherCLI::Messages::AddMessage;
		copydata.cbData = sizeof(data);
		copydata.lpData = &data;

		SendMessageA(Local::LauncherCLI, WM_COPYDATA, 0, (LPARAM)&copydata);
	}
}

void SDR::Library::Load()
{
	auto localdata = std::make_unique<LoadFuncData>();

	try
	{
		localdata->Create(SDR::LauncherCLI::StageType::Load);

		SDR::Setup();
		SDR::Log::Message("{dark}SDR: {green}Source Demo Render loaded\n");

		/*
			Give all output to the game console now.
		*/
		Console::Load();

		localdata->Failure = false;
	}

	catch (const SDR::Error::Exception& error)
	{
		return;
	}
}

const char* SDR::Library::GetGamePath()
{
	return Local::GamePath.c_str();
}

const char* SDR::Library::GetResourcePath()
{
	return Local::ResourcePath.c_str();
}

std::string SDR::Library::BuildResourcePath(const char* file)
{
	std::string ret = GetResourcePath();
	ret += file;

	return ret;
}

extern "C"
{
	__declspec(dllexport) int __cdecl SDR_LibraryVersion()
	{
		return LibraryVersion;
	}

	/*
		First actual pre-engine load function. Don't reference any
		engine libraries here as they aren't loaded yet like in "Load".
	*/
	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::Library::InitializeData& data)
	{
		Local::ResourcePath = data.ResourcePath;
		Local::GamePath = data.GamePath;
		Local::LauncherCLI = data.LauncherCLI;

		SDR::Error::SetPrintFormat("SDR: %s\n");

		/*
			Temporary communication gates. All text output has to go to the launcher window.
		*/
		SDR::Log::SetMessageFunction([](const char* text)
		{
			WriteToLauncherCLI(text);
		});

		SDR::Log::SetMessageColorFunction([](SDR::Shared::Color color, const char* text)
		{
			WriteToLauncherCLI(text);
		});

		SDR::Log::SetWarningFunction([](const char* text)
		{
			auto format = SDR::String::Format("{red}%s", text);
			WriteToLauncherCLI(format.c_str());
		});

		auto localdata = std::make_unique<LoadFuncData>();

		try
		{
			localdata->Create(SDR::LauncherCLI::StageType::Initialize);
			
			SDR::PreEngineSetup();

			localdata->Failure = false;
		}

		catch (const SDR::Error::Exception& error)
		{
			return;
		}

		RegisterLAV();
	}
}
