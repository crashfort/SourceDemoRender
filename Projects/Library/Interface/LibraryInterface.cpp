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
		LibraryVersion = 28,
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
	}

	void RegisterLAV()
	{
		avcodec_register_all();
		av_register_all();
	}

	/*
		Creation has to be delayed as the necessary console stuff isn't available earlier.
	*/
	SDR::StartupFunctionAdder A1("LibraryInterface console commands", []()
	{
		SDR::Console::MakeCommand("sdr_version", Commands::Version);
	});

	struct LoadFuncData : SDR::LauncherCLI::ShadowState
	{
		void Create(SDR::LauncherCLI::StageType stage)
		{
			auto successname = SDR::LauncherCLI::CreateEventSuccessName(stage);
			auto failname = SDR::LauncherCLI::CreateEventFailureName(stage);

			EventSuccess.Attach(OpenEventA(EVENT_MODIFY_STATE, false, successname.c_str()));
			EventFailure.Attach(OpenEventA(EVENT_MODIFY_STATE, false, failname.c_str()));
		}

		~LoadFuncData()
		{
			if (Failure)
			{
				SetEvent(EventFailure.Get());
			}

			else
			{
				SetEvent(EventSuccess.Get());
			}
		}

		bool Failure = false;
	};

	void WriteToLauncherCLI(const std::string& text)
	{
		SDR::LauncherCLI::AddMessageData data;
		strcpy_s(data.Text, text.c_str());

		COPYDATASTRUCT copydata = {};
		copydata.dwData = SDR::LauncherCLI::Messages::AddMessage;
		copydata.cbData = sizeof(data);
		copydata.lpData = &data;

		SendMessageA(Local::LauncherCLI, WM_COPYDATA, 0, (LPARAM)&copydata);
	}

	auto CreateShadowLoadState(SDR::LauncherCLI::StageType stage)
	{
		auto localdata = std::make_unique<LoadFuncData>();
		localdata->Create(stage);

		return localdata;
	}
}

void SDR::Library::Load()
{
	/*
		Temporary communication gates. All text output has to go to the launcher console.
	*/
	SDR::Log::SetMessageFunction([](std::string&& text)
	{
		WriteToLauncherCLI(text);
	});

	SDR::Log::SetMessageColorFunction([](SDR::Shared::Color color, std::string&& text)
	{
		WriteToLauncherCLI(text);
	});

	SDR::Log::SetWarningFunction([](std::string&& text)
	{
		auto format = SDR::String::Format("{red}%s", text.c_str());
		WriteToLauncherCLI(format);
	});

	auto localdata = CreateShadowLoadState(SDR::LauncherCLI::StageType::Load);

	try
	{
		SDR::Setup();
		SDR::Log::Message("{dark}SDR: {green}Source Demo Render loaded\n");

		/*
			Give all output to the game console now.
		*/
		Console::Load();
	}

	catch (const SDR::Error::Exception& error)
	{
		localdata->Failure = true;
	}
}

void SDR::Library::Unload()
{
	SDR::Close();
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

		auto localdata = CreateShadowLoadState(SDR::LauncherCLI::StageType::Initialize);

		try
		{
			SDR::PreEngineSetup();
		}

		catch (const SDR::Error::Exception& error)
		{
			localdata->Failure = true;
			return;
		}

		RegisterLAV();
	}
}
