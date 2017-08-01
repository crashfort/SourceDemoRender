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
			using GetGameDir = void(__cdecl*)(char* dest, int length);
		}

		char FullPath[1024];
		char GameName[128];

		Types::GetGameDir GetGameDir;

		void Set()
		{
			/*
				Assume that this pattern will always exist.
				It's deep in the engine that probably will never be changed anyway.
			*/
			auto patternstr = "55 8B EC 8B 45 08 85 C0 74 11 FF 75 0C 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 0C 5D C3";
			auto pattern = SDR::GetPatternFromString(patternstr);

			auto address = SDR::GetAddressFromPattern("engine.dll", pattern);
			SDR::ModuleShared::SetFromAddress(GetGameDir, address);

			if (!GetGameDir)
			{
				SDR::Error::Make("Could not find current game name");
			}
		}
	}
}

namespace
{
	class SourceDemoRenderPlugin final : public IServerPluginCallbacks
	{
	public:
		virtual bool Load(CreateInterfaceFn interfacefactory, CreateInterfaceFn gameserverfactory) override;
		virtual void Unload() override;

		virtual void Pause() override {}
		virtual void UnPause() override {}

		virtual const char* GetPluginDescription() override
		{
			return "Source Demo Render";
		}

		virtual void LevelInit(char const* mapname) override {}
		virtual void ServerActivate(edict_t* edictlist, int edictcount, int maxclients) override {}
		virtual void GameFrame(bool simulating ) override {}
		virtual void LevelShutdown() override {}
		virtual void ClientActive(edict_t* entity) override {}
		virtual void ClientDisconnect(edict_t* entity) override {}		
		virtual void ClientPutInServer(edict_t* entity, char const* playername) override {}
		virtual void SetCommandClient(int index) override {}
		virtual void ClientSettingsChanged(edict_t* entity) override {}

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

		virtual PLUGIN_RESULT ClientCommand(edict_t* entity, const CCommand& args) override
		{
			return PLUGIN_CONTINUE;
		}

		virtual PLUGIN_RESULT NetworkIDValidated(const char* username, const char* networkid) override
		{
			return PLUGIN_CONTINUE;
		}

		virtual void OnQueryCvarValueFinished
		(
			QueryCvarCookie_t cookie,
			edict_t* playerentity,
			EQueryCvarValueStatus status,
			const char* cvarname,
			const char* cvarvalue
		) override {}

		virtual void OnEdictAllocated(edict_t* entity) override {}
		virtual void OnEdictFreed(const edict_t* entity) override {}

		enum
		{
			PluginVersion = 18,
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
			SDR::Error::Make("Could not get GameConfig version");
		}

		return ret;
	}

	namespace Commands
	{
		CON_COMMAND(sdr_version, "")
		{
			Msg("SDR: Library version: %d\n", SourceDemoRenderPlugin::PluginVersion);

			try
			{
				auto gcversion = GetGameConfigVersion();
				Msg("SDR: Game config version: %d\n", gcversion);
			}

			catch (const SDR::Error::Exception& error)
			{
				return;
			}
		}

		concurrency::task<void> UpdateProc()
		{
			Msg("SDR: Checking for any available updates\n");

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
						Warning("SDR: Could not reach update repository\n");
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

					auto curversion = SourceDemoRenderPlugin::PluginVersion;
					auto webversion = std::stoi(string);

					auto green = Color(88, 255, 39, 255);
					auto blue = Color(51, 167, 255, 255);

					if (curversion == webversion)
					{
						ConColorMsg(green, "SDR: Using the latest library version\n");
					}

					else if (curversion < webversion)
					{
						ConColorMsg
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
						Msg("SDR: Local library newer than update repository?\n");
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
					Msg("SDR: %s", error.what());
				}
			});
		}

		CON_COMMAND(sdr_update, "")
		{
			UpdateProc();
		}
	}

	SourceDemoRenderPlugin ThisPlugin;
	SDR::EngineInterfaces Interfaces;

	template <typename T>
	void CreateInterface(CreateInterfaceFn interfacefactory, const char* name, T*& outptr)
	{
		auto ptr = static_cast<T*>(interfacefactory(name, nullptr));

		if (!ptr)
		{
			SDR::Error::Make("Could not get the %s interface");
		}

		outptr = ptr;
	}

	bool SourceDemoRenderPlugin::Load(CreateInterfaceFn interfacefactory, CreateInterfaceFn gameserverfactory)
	{
		try
		{
			ModuleGameDir::Set();
		}

		catch (const SDR::Error::Exception& error)
		{
			auto version = SourceDemoRenderPlugin::PluginVersion;

			Warning("SDR: Could not get game name. Version: %d\n", version);
			return false;
		}

		ModuleGameDir::GetGameDir(ModuleGameDir::FullPath, sizeof(ModuleGameDir::FullPath));

		strcpy_s(ModuleGameDir::GameName, V_GetFileName(ModuleGameDir::FullPath));
		strcat_s(ModuleGameDir::FullPath, "\\");

		Commands::sdr_version({});

		Msg("SDR: Current game: %s\n", ModuleGameDir::GameName);

		avcodec_register_all();
		av_register_all();

		ConnectTier1Libraries(&interfacefactory, 1);
		ConnectTier2Libraries(&interfacefactory, 1);
		ConVar_Register();

		try
		{
			CreateInterface(interfacefactory, VENGINE_CLIENT_INTERFACE_VERSION, Interfaces.EngineClient);

			SDR::Setup(ModuleGameDir::FullPath, ModuleGameDir::GameName);
			SDR::CallPluginStartupFunctions();
		}

		catch (const SDR::Error::Exception& error)
		{
			return false;
		}

		ConColorMsg(Color(88, 255, 39, 255), "SDR: Source Demo Render loaded\n");
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
