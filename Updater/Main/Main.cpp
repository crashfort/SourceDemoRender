#include <conio.h>
#include <chrono>
#include <string>
#include <cpprest\http_client.h>
#include "Shared\File.hpp"

using namespace std::literals;

namespace
{
	void CatchDeferredErrors(concurrency::task<void> task)
	{
		try
		{
			task.wait();
		}

		catch (const std::exception& error)
		{
			printf_s("SDR: %s", error.what());
		}
	}

	/*
		Downloads the latest game config files from Github.

		Files:
		https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Output/SDR/GameConfigLatest
		https://raw.githubusercontent.com/crashfort/SourceDemoRender/master/Output/SDR/GameConfig.json

		Will place them in the running directory.
	*/
	void UpdateProc()
	{
		printf_s("SDR: Checking for any available updates\n");

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
					printf_s("SDR: Could not reach update repository\n");
					return;
				}

				auto ready = response.content_ready();
				callback(ready.get());
			});

			return task;
		};

		auto gcreq = webrequest
		(
			L"/crashfort/SourceDemoRender/master/Output/SDR/GameConfigLatest",
			[=](web::http::http_response&& response)
			{
				/*
					Content is only text, so extract it raw.
				*/
				auto string = response.extract_utf8string(true).get();

				auto webversion = std::stoi(string);

				auto req = webrequest
				(
					L"/crashfort/SourceDemoRender/master/Output/SDR/GameConfig.json",
					[=](web::http::http_response&& response)
					{
						using Status = SDR::Shared::ScopedFile::ExceptionType;

						/*
							Content is only text, so extract it raw.
						*/
						auto string = response.extract_utf8string(true).get();

						try
						{									
							SDR::Shared::ScopedFile file(L"GameConfig.json", L"wb");
							file.WriteText(string.c_str());
						}

						catch (Status status)
						{
							printf_s("SDR: Could not write GameConfig.json\n");
							return;
						}

						try
						{
							SDR::Shared::ScopedFile file(L"GameConfigLatest", L"wb");
							file.WriteText("%d", webversion);
						}

						catch (Status status)
						{
							printf_s("SDR: Could not write GameConfigLatest\n");
							return;
						}

						printf_s("SDR: Game config download complete\n");
						printf_s("SDR: Now using version %d game config\n", webversion);
					}
				);

				CatchDeferredErrors(req);
			}
		);

		CatchDeferredErrors(gcreq);
	}
}

void wmain(int argc, wchar_t* args[])
{
	UpdateProc();

	printf_s("You can close this window now\n");

	_getch();
}
