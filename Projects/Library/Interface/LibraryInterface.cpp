#include "PrecompiledHeader.hpp"
#include "LibraryInterface.hpp"
#include "Application\Application.hpp"
#include <SDR Library API\ExportTypes.hpp>
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
		LibraryVersion = 27,
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

	struct LoadFuncData : SDR::LauncherCLI::Load::ShadowState
	{
		void Create(SDR::LauncherCLI::Load::StageType stage)
		{
			auto successname = SDR::LauncherCLI::Load::CreateEventSuccessName(stage);
			auto failname = SDR::LauncherCLI::Load::CreateEventFailureName(stage);

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

		void Write(const std::string& text)
		{
			SDR::LauncherCLI::AddMessageData data;
			strcpy_s(data.Text, text.c_str());

			COPYDATASTRUCT copydata = {};
			copydata.dwData = SDR::LauncherCLI::Messages::AddMessage;
			copydata.cbData = sizeof(data);
			copydata.lpData = &data;

			SendMessageA(Local::LauncherCLI, WM_COPYDATA, 0, (LPARAM)&copydata);
		}

		HMODULE LauncherCLI;
		bool Failure = false;
	};

	auto CreateShadowLoadState(SDR::LauncherCLI::Load::StageType stage)
	{
		static LoadFuncData* LoadDataPtr;

		auto localdata = std::make_unique<LoadFuncData>();
		localdata->Create(stage);

		LoadDataPtr = localdata.get();

		/*
			Temporary communication gates. All text output has to go to the launcher console.
		*/
		SDR::Log::SetMessageFunction([](std::string&& text)
		{
			LoadDataPtr->Write(text);
		});

		SDR::Log::SetMessageColorFunction([](SDR::Shared::Color color, std::string&& text)
		{
			LoadDataPtr->Write(text);
		});

		SDR::Log::SetWarningFunction([](std::string&& text)
		{
			auto format = SDR::String::Format("{red}%s{/red}", text.c_str());
			LoadDataPtr->Write(format.c_str());
		});

		return localdata;
	}
}

void SDR::Library::Load()
{
	auto localdata = CreateShadowLoadState(SDR::LauncherCLI::Load::StageType::Load);

	try
	{
		SDR::Setup();
		SDR::Log::Message("{dark}SDR: {/dark}{green}Source Demo Render loaded\n{/green}");

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
	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::API::InitializeData& data)
	{
		Local::ResourcePath = data.ResourcePath;
		Local::GamePath = data.GamePath;
		Local::LauncherCLI = data.LauncherCLI;

		SDR::Error::SetPrintFormat("SDR: %s\n");

		auto localdata = CreateShadowLoadState(SDR::LauncherCLI::Load::StageType::Initialize);

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
