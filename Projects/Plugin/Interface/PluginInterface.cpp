#include "PrecompiledHeader.hpp"
#include "PluginInterface.hpp"
#include "Application\Application.hpp"
#include "SDR Plugin API\ExportTypes.hpp"

extern "C"
{
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
}

#include <cpprest\http_client.h>

namespace
{
	namespace ModuleGameDir
	{
		std::string FullPath;
		std::string GameName;
	}
}

namespace
{
	enum
	{
		PluginVersion = 19,
	};
}

namespace
{
	int GetGameConfigVersion()
	{
		int ret;

		try
		{
			SDR::File::ScopedFile config(SDR::BuildPath("SDR\\GameConfigLatest"), "rb");

			auto str = config.ReadString();

			ret = std::stoi(str);
		}

		catch (SDR::File::ScopedFile::ExceptionType status)
		{
			SDR::Error::Make("Could not get GameConfig version"s);
		}

		return ret;
	}

	namespace Commands
	{
		void Version()
		{
			SDR::Log::Message("SDR: Library version: %d\n", PluginVersion);

			try
			{
				auto gcversion = GetGameConfigVersion();
				SDR::Log::Message("SDR: Game config version: %d\n", gcversion);
			}

			catch (const SDR::Error::Exception& error)
			{
				return;
			}
		}

		concurrency::task<void> UpdateProc()
		{
			SDR::Log::Message("SDR: Checking for any available updates\n"s);

			auto webrequest = [](const wchar_t* path, auto callback)
			{
				auto task = concurrency::create_task([=]()
				{
					auto address = L"https://raw.githubusercontent.com/";

					web::http::client::http_client_config config;
					config.set_timeout(5s);

					web::http::client::http_client webclient(address, config);

					web::http::uri_builder pathbuilder(path);

					auto task = webclient.request(web::http::methods::GET, pathbuilder.to_string());
					auto response = task.get();

					auto status = response.status_code();

					if (status != web::http::status_codes::OK)
					{
						SDR::Log::Warning("SDR: Could not reach update repository\n"s);
						return;
					}

					auto ready = response.content_ready();
					callback(ready.get());
				});

				return task;
			};

			auto libreq = webrequest
			(
				L"/crashfort/SourceDemoRender/master/Version/Latest",
				[](web::http::http_response&& response)
				{
					/*
						Content is only text, so extract it raw.
					*/
					auto string = response.extract_utf8string(true).get();

					auto curversion = PluginVersion;
					auto webversion = std::stoi(string);

					auto green = SDR::Shared::Color(88, 255, 39, 255);
					auto blue = SDR::Shared::Color(51, 167, 255, 255);

					if (curversion == webversion)
					{
						SDR::Log::MessageColor(green, "SDR: Using the latest library version\n");
					}

					else if (curversion < webversion)
					{
						SDR::Log::MessageColor
						(
							blue,
							"SDR: A library update is available: (%d -> %d).\n"
							"Visit https://github.com/crashfort/SourceDemoRender/releases\n",
							curversion,
							webversion
						);
					}

					else if (curversion > webversion)
					{
						SDR::Log::Message("SDR: Local library newer than update repository?\n"s);
					}
				}
			);

			return concurrency::create_task([libreq]()
			{
				try
				{
					libreq.wait();
				}

				catch (const std::exception& error)
				{
					SDR::Log::Message("SDR: %s", error.what());
				}
			});
		}

		void Update()
		{
			UpdateProc();
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
	SDR::PluginStartupFunctionAdder A1("PluginInterface: Console commands", []()
	{
		SDR::Console::MakeCommand("sdr_version", Commands::Version);
		SDR::Console::MakeCommand("sdr_update", Commands::Update);

		return true;
	});

	struct LoadFuncData
	{
		LoadFuncData()
		{
			EventSuccess = OpenEventA(EVENT_MODIFY_STATE, false, "SDR_LOADER_SUCCESS");
			EventFailure = OpenEventA(EVENT_MODIFY_STATE, false, "SDR_LOADER_FAIL");
			Pipe = CreateFileA(R"(\\.\pipe\sdr_loader_pipe)", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		}
		
		~LoadFuncData()
		{
			if (Failure)
			{
				SetEvent(EventFailure);
			}

			else
			{
				SetEvent(EventSuccess);
			}

			CloseHandle(EventSuccess);
			CloseHandle(EventFailure);
			CloseHandle(Pipe);
		}

		void Write(const std::string& text)
		{
			DWORD written;
			WriteFile(Pipe, text.c_str(), text.size(), &written, nullptr);
		}

		HANDLE Pipe;
		
		bool Failure = false;
		HANDLE EventSuccess;
		HANDLE EventFailure;
	};
}

bool SDR::Plugin::Load()
{
	static LoadFuncData* LoadDataPtr;

	auto localdata = std::make_unique<LoadFuncData>();
	LoadDataPtr = localdata.get();

	/*
		Temporary communication gates. All text output has to go to the launcher console.
	*/
	SDR::Log::SetMessageFunction([](std::string&& text)
	{
		LoadDataPtr->Write(text);
	});

	SDR::Log::SetMessageColorFunction([](SDR::Shared::Color col, std::string&& text)
	{
		LoadDataPtr->Write(text);
	});

	SDR::Log::SetWarningFunction([](std::string&& text)
	{
		LoadDataPtr->Write(text);
	});

	try
	{
		SDR::Setup(SDR::GetGamePath(), SDR::GetGameName());

		SDR::CallPluginStartupFunctions();

		Commands::Version();
		SDR::Log::Message("SDR: Current game: %s\n", SDR::GetGameName());
		SDR::Log::MessageColor({ 88, 255, 39 }, "SDR: Source Demo Render loaded\n");

		/*
			Give all output to the game console now.
		*/
		Console::Load();
	}

	catch (const SDR::Error::Exception& error)
	{
		localdata->Failure = true;
		return false;
	}

	return true;
}

void SDR::Plugin::Unload()
{
	SDR::Close();
}

const char* SDR::GetGameName()
{
	return ModuleGameDir::GameName.c_str();
}

const char* SDR::GetGamePath()
{
	return ModuleGameDir::FullPath.c_str();
}

bool SDR::IsGame(const char* test)
{
	return strcmp(GetGameName(), test) == 0;
}

std::string SDR::BuildPath(const char* file)
{
	std::string ret = GetGamePath();
	ret += file;

	return ret;
}

extern "C"
{
	__declspec(dllexport) int __cdecl SDR_LibraryVersion()
	{
		return PluginVersion;
	}

	/*
		First actual pre-engine load function. Don't reference any
		engine libraries here as they aren't loaded yet like in "Load".
	*/
	__declspec(dllexport) SDR::API::InitializeCode __cdecl SDR_Initialize(const char* path, const char* game)
	{
		ModuleGameDir::FullPath = path;
		ModuleGameDir::GameName = game;

		SDR::Error::SetPrintFormat("SDR: %s\n");

		RegisterLAV();

		try
		{
			SDR::PreEngineSetup();
		}

		catch (SDR::API::InitializeCode code)
		{
			return code;
		}

		return SDR::API::InitializeCode::Success;
	}
}
