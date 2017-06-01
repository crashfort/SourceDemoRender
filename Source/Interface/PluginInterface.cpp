#include "PrecompiledHeader.hpp"
#include "Application\Application.hpp"

extern "C"
{
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
}

#include <cpprest\http_client.h>

namespace
{
	class SourceDemoRenderPlugin final : public IServerPluginCallbacks
	{
	public:
		virtual bool Load
		(
			CreateInterfaceFn interfacefactory,
			CreateInterfaceFn gameserverfactory
		) override;

		virtual void Unload
		(

		) override;

		virtual void Pause
		(

		) override {}

		virtual void UnPause
		(

		) override {}

		virtual const char* GetPluginDescription() override
		{
			return "Source Demo Render";
		}

		virtual void LevelInit
		(
			char const* mapname
		) override {}

		virtual void ServerActivate
		(
			edict_t* edictlist,
			int edictcount,
			int maxclients
		) override {}

		virtual void GameFrame
		(
			bool simulating
		) override {}

		virtual void LevelShutdown
		(

		) override {}

		virtual void ClientActive
		(
			edict_t* entity
		) override {}

		virtual void ClientDisconnect
		(
			edict_t* entity
		) override {}
		
		virtual void ClientPutInServer
		(
			edict_t* entity,
			char const* playername
		) override {}

		virtual void SetCommandClient
		(
			int index
		) override {}

		virtual void ClientSettingsChanged
		(
			edict_t* entity
		) override {}

		virtual PLUGIN_RESULT ClientConnect
		(
			bool* allowconnect,
			edict_t* entity,
			const char* name,
			const char* address,
			char* rejectreason,
			int maxrejectlen
		) override
		{
			return PLUGIN_CONTINUE;
		}

		virtual PLUGIN_RESULT ClientCommand
		(
			edict_t* entity,
			const CCommand& args
		) override
		{
			return PLUGIN_CONTINUE;
		}

		virtual PLUGIN_RESULT NetworkIDValidated
		(
			const char* username,
			const char* networkid
		) override
		{
			return PLUGIN_CONTINUE;
		}

		virtual void OnQueryCvarValueFinished
		(
			QueryCvarCookie_t cookie,
			edict_t* playerentity,
			EQueryCvarValueStatus status,
			const char *cvarname,
			const char *cvarvalue
		) override {}

		virtual void OnEdictAllocated
		(
			edict_t* entity
		) override {}

		virtual void OnEdictFreed
		(
			const edict_t* entity
		) override {}

		enum
		{
			PluginVersion = 10
		};
	};

	namespace Commands
	{
		CON_COMMAND(sdr_version, "Show the current version")
		{
			Msg
			(
				"SDR: Current version: %d\n",
				SourceDemoRenderPlugin::PluginVersion
			);
		}

		CON_COMMAND(sdr_update, "Check for any available updates")
		{
			Msg("SDR: Checking for any available update\n");

			auto task = concurrency::create_task([]()
			{
				constexpr auto address = L"https://raw.githubusercontent.com/";
				constexpr auto path = L"/crashfort/SourceDemoRender/master/Version/Latest";

				web::http::client::http_client_config config;
				config.set_timeout(5s);

				web::http::client::http_client webclient(address, config);

				web::http::uri_builder pathbuilder(path);

				return webclient.request(web::http::methods::GET, pathbuilder.to_string());
			});

			task.then([](web::http::http_response response)
			{
				auto status = response.status_code();

				if (status != web::http::status_codes::OK)
				{
					Msg("SDR Update: Could not reach update repository\n");
					return;
				}

				auto task = response.content_ready();

				task.then([](web::http::http_response response)
				{
					/*
						Content is only text containg a number, so extract it raw
					*/
					auto task = response.extract_string(true);

					task.then([](utility::string_t string)
					{
						constexpr auto curversion = SourceDemoRenderPlugin::PluginVersion;
						auto webversion = std::stoi(string);

						if (curversion == webversion)
						{
							ConColorMsg
							(
								Color(88, 255, 39, 255),
								"SDR Update: Using the latest version\n"
							);
						}

						else if (curversion < webversion)
						{
							ConColorMsg
							(
								Color(51, 167, 255, 255),
								"SDR Update: An update is available. New version: %d, current: %d\n"
								"Visit https://github.com/CRASHFORT/SourceDemoRender/releases\n",
								webversion, curversion
							);
						}

						else if (curversion > webversion)
						{
							Msg
							(
								"SDR Update: Current version greater than update repository?\n"
							);
						}
					});
				});
			});
		}
	}

	SourceDemoRenderPlugin ThisPlugin;
	SDR::EngineInterfaces Interfaces;

	template <typename T>
	void CreateInterface
	(
		CreateInterfaceFn interfacefactory,
		const char* name,
		T*& outptr
	)
	{
		auto ptr = static_cast<T*>(interfacefactory(name, nullptr));

		if (!ptr)
		{
			throw name;
		}

		outptr = ptr;
	}

	bool SourceDemoRenderPlugin::Load
	(
		CreateInterfaceFn interfacefactory,
		CreateInterfaceFn gameserverfactory
	)
	{
		Msg
		(
			"SDR: Current version: %d\n",
			SourceDemoRenderPlugin::PluginVersion
		);

		{








		try
		{
			CreateInterface
			(
				interfacefactory,
				VENGINE_CLIENT_INTERFACE_VERSION,
				Interfaces.EngineClient
			);
		}

		catch (const char* name)
		{
			Warning
			(
				"SDR: Failed to get the \"%s\" interface\n",
				name
			);

			return false;
		}

		try
		{
			SDR::Setup();
		}

		catch (MH_STATUS status)
		{
			return false;
		}

		try
		{
			SDR::CallPluginStartupFunctions();
		}

		catch (const char* name)
		{
			Warning
			(
				"SDR: Setup procedure \"%s\" failed\n",
				name
			);

			return false;
		}

		Msg
		(
			"SDR: Source Demo Render loaded\n"
		);

		return true;
	}

	void SourceDemoRenderPlugin::Unload()
	{
		SDR::Close();

		ConVar_Unregister();
		DisconnectTier1Libraries();
		DisconnectTier2Libraries();
	}
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR
(
	SourceDemoRenderPlugin,
	IServerPluginCallbacks,
	INTERFACEVERSION_ISERVERPLUGINCALLBACKS,
	ThisPlugin
);

const SDR::EngineInterfaces& SDR::GetEngineInterfaces()
{
	return Interfaces;
}
