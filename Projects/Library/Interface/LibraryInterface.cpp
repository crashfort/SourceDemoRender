#include "PrecompiledHeader.hpp"
#include "LibraryInterface.hpp"
#include "Application\Application.hpp"
#include "SDR Library API\ExportTypes.hpp"

extern "C"
{
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
}

#include <cpprest\http_client.h>
#include <shellapi.h>

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
		LibraryVersion = 24,
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

					auto curversion = LibraryVersion;
					auto webversion = std::stoi(string);

					auto green = SDR::Shared::Color(88, 255, 39);
					auto blue = SDR::Shared::Color(51, 167, 255);

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
							"Use \"sdr_accept\" to open Github release page\n",
							curversion,
							webversion
						);

						SDR::Library::SetAcceptFunction([=]()
						{
							SDR::Log::Message("SDR: Upgrading from version %d to %d\n", curversion, webversion);

							auto address = L"https://github.com/crashfort/SourceDemoRender/releases";
							ShellExecuteW(nullptr, L"open", address, nullptr, nullptr, SW_SHOW);
						});
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

		namespace Accept
		{
			std::function<void()> Callback;

			void Procedure()
			{
				if (Callback)
				{
					Callback();
					Callback = nullptr;
				}

				else
				{
					SDR::Log::Message("SDR: Nothing to accept\n");
				}
			}
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
		SDR::Console::MakeCommand("sdr_update", Commands::Update);
		SDR::Console::MakeCommand("sdr_accept", Commands::Accept::Procedure);
	});

	struct LoadFuncData : SDR::API::ShadowState
	{
		void Create(SDR::API::StageType stage)
		{
			auto pipename = SDR::API::CreatePipeName(stage);
			auto successname = SDR::API::CreateEventSuccessName(stage);
			auto failname = SDR::API::CreateEventFailureName(stage);

			Pipe.Attach(CreateFileA(pipename.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
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
			DWORD written;
			WriteFile(Pipe.Get(), text.c_str(), text.size(), &written, nullptr);
		}

		bool Failure = false;
	};

	auto CreateShadowLoadState(SDR::API::StageType stage)
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

		SDR::Log::SetMessageColorFunction([](SDR::Shared::Color col, std::string&& text)
		{
			LoadDataPtr->Write(text);
		});

		SDR::Log::SetWarningFunction([](std::string&& text)
		{
			LoadDataPtr->Write(text);
		});

		return localdata;
	}
}

void SDR::Library::Load()
{
	auto localdata = CreateShadowLoadState(SDR::API::StageType::Load);

	try
	{
		SDR::Setup(SDR::Library::GetGamePath(), SDR::Library::GetGameName());
		SDR::Log::Message("SDR: Source Demo Render loaded\n");

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

void SDR::Library::SetAcceptFunction(std::function<void()>&& func)
{
	Commands::Accept::Callback = std::move(func);
}

const char* SDR::Library::GetGameName()
{
	return ModuleGameDir::GameName.c_str();
}

const char* SDR::Library::GetGamePath()
{
	return ModuleGameDir::FullPath.c_str();
}

bool SDR::Library::IsGame(const char* test)
{
	return strcmp(GetGameName(), test) == 0;
}

std::string SDR::Library::BuildPath(const char* file)
{
	std::string ret = GetGamePath();
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
	__declspec(dllexport) void __cdecl SDR_Initialize(const char* path, const char* game)
	{
		ModuleGameDir::FullPath = path;
		ModuleGameDir::GameName = game;

		SDR::Error::SetPrintFormat("SDR: %s\n");

		auto localdata = CreateShadowLoadState(SDR::API::StageType::Initialize);

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
