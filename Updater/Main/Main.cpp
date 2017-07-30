#include <Windows.h>
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

	void UpdateProc(const std::wstring& dir)
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
							auto path = dir + L"GameConfig.json";
									
							SDR::Shared::ScopedFile file(path.c_str(), L"wb");

							file.WriteText(string.c_str());
						}

						catch (Status status)
						{
							printf_s("SDR: Could not write GameConfig.json\n");
							return;
						}

						try
						{
							auto path = dir + L"GameConfigLatest";

							SDR::Shared::ScopedFile file(path.c_str(), L"wb");

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
	std::wstring dir = args[0];

	auto length = dir.size();

	for (int i = length - 1; i >= 0; i--)
	{
		if (dir[i] == L'\\')
		{
			dir.resize(i + 1);
			break;
		}
	}

	UpdateProc(dir);

	printf_s("You can close this window now\n");

	std::getchar();
}
