#include "PrecompiledHeader.hpp"
#include "Application\Application.hpp"

extern "C"
{
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
}

#include <cpprest\http_client.h>

namespace LAV
{
	void LogFunction
	(
		void* avcl,
		int level,
		const char* fmt,
		va_list vl
	);
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

		virtual void Unload() override;

		virtual void Pause() override {}
		virtual void UnPause() override {}

		virtual const char* GetPluginDescription() override
		{
			return "Source Demo Render";
		}

		virtual void LevelInit(char const* mapname) override {}

		virtual void ServerActivate
		(
			edict_t* edictlist, int edictcount, int maxclients
		) override {}

		virtual void GameFrame(bool simulating) override {}

		virtual void LevelShutdown() override {}

		virtual void ClientActive(edict_t* entity) override {}
		virtual void ClientDisconnect(edict_t* entity) override {}
		
		virtual void ClientPutInServer
		(
			edict_t* entity,
			char const* playername
		) override {}

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
		) override
		{

		}

		virtual void OnEdictAllocated(edict_t* entity) override {}
		virtual void OnEdictFreed(const edict_t* entity) override {}

		enum
		{
			PluginVersion = 4
		};
	};

	namespace Commands
	{
		CON_COMMAND(sdr_version, "Show the current version")
		{
			Msg("SDR: Current version: %d\n", SourceDemoRenderPlugin::PluginVersion);
		}

		CON_COMMAND(sdr_update, "Check for any available updates")
		{
			Msg("SDR: Checking for any available update\n");

			auto task = concurrency::create_task([]()
			{
				constexpr auto address = L"https://raw.githubusercontent.com/";
				constexpr auto path = L"/CRASHFORT/SourceDemoRender/master/Version/Latest";

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
					Msg("SDR Update: Could not reach update repository");
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

	class InterfaceBase
	{
	public:
		InterfaceBase(const char* interfacename) :
			InterfaceName(interfacename)
		{
			
		}

		virtual bool Create
		(
			CreateInterfaceFn interfacefactory,
			CreateInterfaceFn gameserverfactory
		) = 0;

		const char* GetInterfaceName() const
		{
			return InterfaceName;
		}

	protected:
		const char* InterfaceName;
	};

	template <typename T>
	class InterfaceFactoryLocal final : public InterfaceBase
	{
	public:
		InterfaceFactoryLocal(T** targetinterface, const char* interfacename) :
			InterfaceBase(interfacename),
			TargetInterface(targetinterface)
		{
			
		}

		virtual bool Create
		(
			CreateInterfaceFn interfacefactory,
			CreateInterfaceFn gameserverfactory
		) override
		{
			*TargetInterface = static_cast<T*>(interfacefactory(InterfaceName, nullptr));

			return TargetInterface != nullptr;
		}

	private:
		T** TargetInterface;
	};

	template <typename T>
	class ServerFactoryLocal final : public InterfaceBase
	{
	public:
		ServerFactoryLocal(T** targetinterface, const char* interfacename) :
			InterfaceBase(interfacename),
			TargetInterface(targetinterface)
		{
			
		}

		virtual bool Create
		(
			CreateInterfaceFn interfacefactory,
			CreateInterfaceFn gameserverfactory
		) override
		{
			*TargetInterface = static_cast<T*>(gameserverfactory(InterfaceName, nullptr));

			return TargetInterface != nullptr;
		}

	private:
		T** TargetInterface;
	};

	bool SourceDemoRenderPlugin::Load
	(
		CreateInterfaceFn interfacefactory,
		CreateInterfaceFn gameserverfactory
	)
	{
		avcodec_register_all();
		av_register_all();

		av_log_set_callback(LAV::LogFunction);

		ConnectTier1Libraries(&interfacefactory, 1);
		ConnectTier2Libraries(&interfacefactory, 1);
		ConVar_Register();

		ServerFactoryLocal<IPlayerInfoManager> T1
		(
			&Interfaces.PlayerInfoManager,
			INTERFACEVERSION_PLAYERINFOMANAGER
		);

		InterfaceFactoryLocal<IVEngineClient> T2
		(
			&Interfaces.EngineClient,
			VENGINE_CLIENT_INTERFACE_VERSION
		);

		InterfaceFactoryLocal<IFileSystem> T3
		(
			&Interfaces.FileSystem,
			FILESYSTEM_INTERFACE_VERSION
		);

		std::initializer_list<InterfaceBase*> list =
		{
			&T1,
			&T2,
			&T3
		};

		for (auto ptr : list)
		{
			auto res = ptr->Create(interfacefactory, gameserverfactory);

			if (!res)
			{
				Warning("SDR: Failed to get the \"%s\" interface\n", ptr->GetInterfaceName());
				return false;
			}
		}

		Interfaces.Globals = Interfaces.PlayerInfoManager->GetGlobalVars();

		auto res = SDR::Setup();

		if (!res)
		{
			return false;
		}

		Msg("SDR: Source Demo Render loaded\n");

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
