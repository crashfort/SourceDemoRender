#include "PrecompiledHeader.hpp"
#include "Interface\LibraryInterface.hpp"
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Hooking.hpp>
#include <SDR Library API\LibraryAPI.hpp>
#include <SDR LauncherCLI API\LauncherCLIAPI.hpp>
#include "Application.hpp"
#include "ConfigSystem.hpp"
#include "Interface\Application\Extensions\ExtensionManager.hpp"

namespace
{
	struct ApplicationData
	{
		void PrintModuleState(bool value, const char* name)
		{
			if (!value)
			{
				SDR::Log::Warning("SDR: No handler found for \"%s\"\n", name);
			}

			else
			{
				SDR::Log::Message("{dark}SDR: {white}Enabled module {string}\"%s\"\n", name);
			}
		}

		void CallGameHandlers(SDR::ConfigSystem::ObjectData* game)
		{
			SDR::Log::Message("{dark}SDR: {white}Creating {number}%d {white}game modules\n", ModuleHandlers.size());

			for (auto& prop : game->Properties)
			{
				/*
					Ignore these, they are only used by the launcher.
				*/
				if (prop.Name == "DisplayName")
				{
					continue;
				}

				else if (prop.Name == "ExecutableName")
				{
					continue;
				}

				bool found = false;

				for (auto& handler : ModuleHandlers)
				{
					if (prop.Name == handler.Name)
					{
						found = true;

						try
						{
							SDR::Error::ScopedContext e1(handler.Name);

							handler.Function(prop.Value);
						}

						catch (const SDR::Error::Exception& error)
						{
							SDR::Error::Make("Could not enable module \"%s\"", handler.Name);
							throw;
						}

						break;
					}
				}

				PrintModuleState(found, prop.Name.c_str());
			}
		}

		void CallExtensionHandlers(SDR::ConfigSystem::ObjectData* object)
		{
			if (object->Properties.empty())
			{
				return;
			}

			std::vector<SDR::ConfigSystem::PropertyData*> temp;

			for (auto& prop : object->Properties)
			{
				if (SDR::ExtensionManager::IsNamespaceLoaded(prop.Name.c_str()))
				{
					temp.emplace_back(&prop);
				}
			}

			if (temp.empty())
			{
				return;
			}

			SDR::Log::Message("{dark}SDR: {white}Creating {number}%d {white}extension modules\n", temp.size());

			for (auto& prop : temp)
			{
				auto found = SDR::ExtensionManager::Events::CallHandlers(prop->Name.c_str(), prop->Value);
				PrintModuleState(found, prop->Name.c_str());
			}
		}

		void SetupGame()
		{
			std::vector<SDR::ConfigSystem::ObjectData> GameConfigs;

			rapidjson::Document document;

			try
			{
				document = SDR::Json::FromFile(SDR::Library::BuildResourcePath("GameConfig.json"));
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find game config"s);
			}

			auto searcher = SDR::Library::GetGamePath();
			auto object = SDR::ConfigSystem::FindAndPopulateObject(document, searcher, GameConfigs);

			if (!object)
			{
				SDR::Error::Make("Could not find current game in game config"s);
			}

			SDR::ConfigSystem::ResolveInherit(object, GameConfigs, document.GetAllocator());
			SDR::ConfigSystem::ResolveSort(object);
			
			CallGameHandlers(object);
			GameConfigs.clear();
		}

		void SetupExtensions()
		{
			std::vector<SDR::ConfigSystem::ObjectData> ExtensionConfigs;

			rapidjson::Document document;

			try
			{
				document = SDR::Json::FromFile(SDR::Library::BuildResourcePath("ExtensionConfig.json"));
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find extension config"s);
			}

			auto searcher = SDR::Library::GetGamePath();
			auto object = FindAndPopulateObject(document, searcher, ExtensionConfigs);

			if (!object)
			{
				SDR::Error::Make("Could not find current game in extension config"s);
			}

			SDR::ConfigSystem::ResolveInherit(object, ExtensionConfigs, document.GetAllocator());
			SDR::ConfigSystem::ResolveSort(object);
			
			CallExtensionHandlers(object);
			ExtensionConfigs.clear();
		}

		void CallStartupFunctions()
		{
			for (auto entry : StartupFunctions)
			{
				try
				{
					SDR::Error::ScopedContext e1(entry.Name);
					entry.Function();
				}

				catch (const SDR::Error::Exception& error)
				{
					SDR::Error::Make("Could not pass startup procedure \"%s\"", entry.Name);
					throw;
				}

				SDR::Log::Message("{dark}SDR: {white}Passed startup procedure: {string}\"%s\"\n", entry.Name);
			}
		}

		std::vector<SDR::ModuleHandlerData> ModuleHandlers;
		std::vector<SDR::StartupFuncData> StartupFunctions;
	};

	ApplicationData ThisApplication;
}

namespace
{
	namespace LoadLibraryIntercept
	{
		namespace Common
		{
			template <typename T>
			using TableType = std::initializer_list<std::pair<const T*, std::function<void()>>>;

			template <typename T>
			void CheckTable(const TableType<T>& table, const T* name)
			{
				for (auto& entry : table)
				{
					if (SDR::String::EndsWith(name, entry.first))
					{
						entry.second();
						break;
					}
				}
			}

			void Load(HMODULE module, const char* name)
			{
				TableType<char> table =
				{
					std::make_pair("server.dll", []()
					{
						/*
							This should be changed in the future.
						*/
						SDR::Library::Load();
					})
				};

				CheckTable(table, name);
			}

			void Load(HMODULE module, const wchar_t* name)
			{
				TableType<wchar_t> table =
				{
					
				};

				CheckTable(table, name);
			}
		}

		namespace A
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryA)*> ThisHook;

			HMODULE WINAPI Override(LPCSTR name)
			{
				auto ret = ThisHook.GetOriginal()(name);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		namespace ExA
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryExA)*> ThisHook;

			HMODULE WINAPI Override(LPCSTR name, HANDLE file, DWORD flags)
			{
				auto ret = ThisHook.GetOriginal()(name, file, flags);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		namespace W
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryW)*> ThisHook;

			HMODULE WINAPI Override(LPCWSTR name)
			{
				auto ret = ThisHook.GetOriginal()(name);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		namespace ExW
		{
			SDR::Hooking::HookModule<decltype(LoadLibraryExW)*> ThisHook;

			HMODULE WINAPI Override(LPCWSTR name, HANDLE file, DWORD flags)
			{
				auto ret = ThisHook.GetOriginal()(name, file, flags);

				if (ret)
				{
					Common::Load(ret, name);
				}

				return ret;
			}
		}

		void Start()
		{
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryA", A::ThisHook, A::Override);
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryExA", ExA::ThisHook, ExA::Override);
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryW", W::ThisHook, W::Override);
			SDR::Hooking::CreateHookAPI(L"kernel32.dll", "LoadLibraryExW", ExW::ThisHook, ExW::Override);

			SDR::Error::MH::ThrowIfFailed
			(
				MH_EnableHook(A::ThisHook.TargetFunction),
				"Could not enable library intercept A"
			);

			SDR::Error::MH::ThrowIfFailed
			(
				MH_EnableHook(ExA::ThisHook.TargetFunction),
				"Could not enable library intercept ExA"			
			);

			SDR::Error::MH::ThrowIfFailed
			(
				MH_EnableHook(W::ThisHook.TargetFunction),
				"Could not enable library intercept W"
			);
			
			SDR::Error::MH::ThrowIfFailed
			(
				MH_EnableHook(ExW::ThisHook.TargetFunction),
				"Could not enable library intercept ExW"
			);
		}

		void End()
		{
			MH_DisableHook(A::ThisHook.TargetFunction);
			MH_DisableHook(ExA::ThisHook.TargetFunction);
			MH_DisableHook(W::ThisHook.TargetFunction);
			MH_DisableHook(ExW::ThisHook.TargetFunction);
		}
	}
}

void SDR::PreEngineSetup()
{
	SDR::Error::MH::ThrowIfFailed
	(
		SDR::Hooking::Initialize(),
		"Could not initialize hooks"
	);

	LoadLibraryIntercept::Start();
}

void SDR::Setup()
{
	LoadLibraryIntercept::End();
	
	ThisApplication.SetupGame();
	ThisApplication.CallStartupFunctions();

	ExtensionManager::LoadExtensions();

	if (SDR::ExtensionManager::HasExtensions())
	{
		ThisApplication.SetupExtensions();
		ExtensionManager::Events::Ready();
	}
}

void SDR::AddStartupFunction(const StartupFuncData& data)
{
	ThisApplication.StartupFunctions.emplace_back(data);
}

void SDR::AddModuleHandler(const ModuleHandlerData& data)
{
	ThisApplication.ModuleHandlers.emplace_back(data);
}
