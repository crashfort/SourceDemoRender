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
	};

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
