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
	namespace ModuleGameDir
	{
		namespace Types
		{
			using GetGameDir = void(__cdecl*)
			(
				char* dest,
				int length
			);
		}

		char FullPath[1024];
		char GameName[128];

		Types::GetGameDir GetGameDir;

		void Set()
		{
			auto patternstr = "55 8B EC 8B 45 08 85 C0 74 11 FF 75 0C 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 0C 5D C3";
			auto pattern = SDR::GetPatternFromString(patternstr);

			auto address = SDR::GetAddressFromPattern
			(
				"engine.dll",
				pattern
			);

			SDR::ModuleShared::SetFromAddress
			(
				GetGameDir,
				address
			);

			SDR::ModuleShared::Verify
			(
				GetGameDir,
				"engine.dll",
				"GetGameDir"
			);
		}
	}
}

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
			PluginVersion = 10,
		};
	};

	int GetGameConfigVersion()
	{
		int ret;

		try
		{
			char path[1024];
			strcpy_s(path, SDR::GetGamePath());
			strcat(path, "SDR\\GameConfigLatest");

			SDR::Shared::ScopedFile config(path, "rb");

			auto content = config.ReadAll();
			std::string str((const char*)content.data(), content.size());

			ret = std::stoi(str);
		}

		catch (SDR::Shared::ScopedFile::ExceptionType status)
		{
			Warning
			(
				"SDR: Could not get GameConfig version\n"
			);

			throw false;
		}

		return ret;
	}

	namespace Commands
	{
		CON_COMMAND(sdr_version, "Show the current version")
		{
			Msg
			(
				"SDR: Library version: %d\n",
				SourceDemoRenderPlugin::PluginVersion
			);

			int gameconfigversion;

			try
			{
				gameconfigversion = GetGameConfigVersion();
			}

			catch (bool value)
			{
				return;
			}

			Msg
			(
				"SDR: GameConfig version: %d\n",
				gameconfigversion
			);
		}

		CON_COMMAND(sdr_update, "Check for any available updates")
		{
			Msg
			(
				"SDR: Checking for any available updates\n"
			);

			auto webrequest = []
			(
				const wchar_t* path,
				auto callback
			)
			{
				auto task = concurrency::create_task([path]()
				{
					auto address = L"https://raw.githubusercontent.com/";

					web::http::client::http_client_config config;
					config.set_timeout(5s);

					web::http::client::http_client webclient(address, config);

					web::http::uri_builder pathbuilder(path);

					return webclient.request
					(
						web::http::methods::GET,
						pathbuilder.to_string()
					);
				});

				task.then([callback](web::http::http_response response)
				{
					auto status = response.status_code();

					if (status != web::http::status_codes::OK)
					{
						Msg
						(
							"SDR: Could not reach update repository\n"
						);

						return;
					}

					auto task = response.content_ready();

					task.then([callback](web::http::http_response response)
					{
						callback(std::move(response));
					});
				});
			};

			webrequest
			(
				L"/crashfort/SourceDemoRender/master/Version/Latest",
				[](web::http::http_response&& response)
				{
					/*
						Content is only text, so extract it raw
					*/
					auto string = response.extract_utf8string(true).get();

					auto curversion = SourceDemoRenderPlugin::PluginVersion;
					auto webversion = std::stoi(string);

					if (curversion == webversion)
					{
						ConColorMsg
						(
							Color(88, 255, 39, 255),
							"SDR: Using the latest library version\n"
						);
					}

					else if (curversion < webversion)
					{
						ConColorMsg
						(
							Color(51, 167, 255, 255),
							"SDR: A library update is available. New version: %d, current: %d\n"
							"Visit https://github.com/CRASHFORT/SourceDemoRender/releases\n",
							webversion,
							curversion
						);
					}

					else if (curversion > webversion)
					{
						Msg
						(
							"SDR: Local library newer than update repository?\n"
						);
					}
				}
			);

			webrequest
			(
				L"/crashfort/SourceDemoRender/master/Version/GameConfigLatest",
				[webrequest](web::http::http_response&& response)
				{
					int localversion;

					try
					{
						localversion = GetGameConfigVersion();
					}

					catch (bool value)
					{
						return;
					}

					/*
						Content is only text, so extract it raw
					*/
					auto string = response.extract_utf8string(true).get();

					auto webversion = std::stoi(string);

					if (localversion == webversion)
					{
						ConColorMsg
						(
							Color(88, 255, 39, 255),
							"SDR: Using the latest game config\n"
						);
					}

					else if (localversion < webversion)
					{
						ConColorMsg
						(
							Color(51, 167, 255, 255),
							"SDR: A game config update is available: (%d -> %d), downloading\n",
							localversion,
							webversion
						);

						webrequest
						(
							L"/crashfort/SourceDemoRender/master/Output/SDR/GameConfig.json",
							[webversion](web::http::http_response&& response)
							{
								using Status = SDR::Shared::ScopedFile::ExceptionType;

								auto json = response.extract_utf8string().get();

								try
								{
									char path[1024];
									strcpy_s(path, SDR::GetGamePath());
									strcat_s(path, "SDR\\GameConfig.json");
									
									SDR::Shared::ScopedFile file(path, "wb");

									file.WriteText("%s", json.c_str());
								}

								catch (Status status)
								{
									Warning
									(
										"SDR: Could not write GameConfig.json\n"
									);

									return;
								}

								try
								{
									char path[1024];
									strcpy_s(path, SDR::GetGamePath());
									strcat_s(path, "SDR\\GameConfigLatest");

									SDR::Shared::ScopedFile file(path, "wb");

									file.WriteText("%d", webversion);
								}

								catch (Status status)
								{
									Warning
									(
										"SDR: Could not write GameConfigLatest\n"
									);

									return;
								}

								ConColorMsg
								(
									Color(51, 167, 255, 255),
									"SDR: Game config download complete\n"
								);
							}
						);
					}

					else if (localversion > webversion)
					{
						Msg
						(
							"SDR: Local game config newer than update repository?\n"
						);
					}
				}
			);
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
		try
		{
			ModuleGameDir::Set();
		}

		catch (const char* name)
		{
			Warning
			(
				"SDR: Could not get game name. Version: %d\n",
				SourceDemoRenderPlugin::PluginVersion
			);

			return false;
		}

		ModuleGameDir::GetGameDir
		(
			ModuleGameDir::FullPath,
			sizeof(ModuleGameDir::FullPath)
		);

		strcpy_s
		(
			ModuleGameDir::GameName,
			V_GetFileName(ModuleGameDir::FullPath)
		);

		strcat_s(ModuleGameDir::FullPath, "\\");

		Commands::sdr_version({});

		Msg
		(
			"SDR: Current game: %s\n",
			ModuleGameDir::GameName
		);

		avcodec_register_all();
		av_register_all();

		ConnectTier1Libraries(&interfacefactory, 1);
		ConnectTier2Libraries(&interfacefactory, 1);
		ConVar_Register();

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
			SDR::Setup
			(
				ModuleGameDir::FullPath,
				ModuleGameDir::GameName
			);
		}

		catch (bool status)
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

const char* SDR::GetGameName()
{
	return ModuleGameDir::GameName;
}

const char* SDR::GetGamePath()
{
	return ModuleGameDir::FullPath;
}
