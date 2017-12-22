#include "PrecompiledHeader.hpp"
#include "Interface\LibraryInterface.hpp"
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Hooking.hpp>
#include <SDR Library API\ExportTypes.hpp>
#include "Application.hpp"
#include "Interface\Application\Extensions\ExtensionManager.hpp"

namespace
{
	struct Application
	{
		std::vector<SDR::ModuleHandlerData> ModuleHandlers;
		std::vector<SDR::StartupFuncData> StartupFunctions;
		std::vector<SDR::ShutdownFuncType> ShutdownFunctions;
	};

	Application MainApplication;

	namespace Config
	{
		template <typename NodeType, typename FuncType>
		void MemberLoop(NodeType& node, FuncType callback)
		{
			auto& begin = node.MemberBegin();
			auto& end = node.MemberEnd();

			for (auto it = begin; it != end; ++it)
			{
				callback(it);
			}
		}

		struct ConfigObjectData
		{
			std::string ObjectName;
			std::vector<std::pair<std::string, rapidjson::Value>> Properties;
		};

		std::vector<ConfigObjectData> GameConfigs;
		std::vector<ConfigObjectData> ExtensionConfigs;

		void ResolveInherit(ConfigObjectData* targetgame, const std::vector<ConfigObjectData>& source, rapidjson::Document::AllocatorType& alloc)
		{
			auto begin = targetgame->Properties.begin();
			auto end = targetgame->Properties.end();

			auto foundinherit = false;

			for (auto it = begin; it != end; ++it)
			{
				if (it->first == "Inherit")
				{
					foundinherit = true;

					if (!it->second.IsString())
					{
						SDR::Error::Make("SDR: \"%s\" inherit field not a string\n", targetgame->ObjectName.c_str());
					}

					std::string from = it->second.GetString();

					targetgame->Properties.erase(it);

					for (const auto& game : source)
					{
						bool foundgame = false;

						if (game.ObjectName == from)
						{
							foundgame = true;

							for (const auto& sourceprop : game.Properties)
							{
								bool shouldadd = true;

								for (const auto& destprop : targetgame->Properties)
								{
									if (sourceprop.first == destprop.first)
									{
										shouldadd = false;
										break;
									}
								}

								if (shouldadd)
								{
									targetgame->Properties.emplace_back(sourceprop.first, rapidjson::Value(sourceprop.second, alloc));
								}
							}

							break;
						}

						if (!foundgame)
						{
							SDR::Error::Make("\"%s\" inherit target \"%s\" not found", targetgame->ObjectName.c_str(), from.c_str());
						}
					}

					break;
				}
			}

			if (foundinherit)
			{
				ResolveInherit(targetgame, source, alloc);
			}
		}

		void PrintModuleState(bool value, const char* name)
		{
			if (!value)
			{
				SDR::Log::Warning("SDR: No handler found for \"%s\"\n", name);
			}

			else
			{
				SDR::Log::Message("SDR: Enabled module \"%s\"\n", name);
			}
		}

		void CallGameHandlers(ConfigObjectData* game)
		{
			SDR::Log::Message("SDR: Creating %d game modules\n", MainApplication.ModuleHandlers.size());

			for (auto& prop : game->Properties)
			{
				/*
					Ignore these, they are only used by the launcher.
				*/
				if (prop.first == "DisplayName")
				{
					continue;
				}

				else if (prop.first == "ExecutableName")
				{
					continue;
				}

				bool found = false;

				for (auto& handler : MainApplication.ModuleHandlers)
				{
					if (prop.first == handler.Name)
					{
						found = true;

						try
						{
							SDR::Error::ScopedContext e1(handler.Name);

							handler.Function(prop.second);
						}

						catch (const SDR::Error::Exception& error)
						{
							SDR::Error::Make("Could not enable module \"%s\"", handler.Name);
							throw;
						}

						break;
					}
				}

				PrintModuleState(found, prop.first.c_str());
			}

			MainApplication.ModuleHandlers.clear();
		}

		void CallExtensionHandlers(ConfigObjectData* object)
		{
			if (object->Properties.empty())
			{
				return;
			}

			SDR::Log::Message("SDR: Creating %d extension modules\n", object->Properties.size());

			for (auto& prop : object->Properties)
			{
				try
				{
					SDR::Error::ScopedContext e1(prop.first.c_str());

					auto found = SDR::ExtensionManager::Events::CallHandlers(prop.first.c_str(), prop.second);
					PrintModuleState(found, prop.first.c_str());
				}

				catch (const SDR::Error::Exception& error)
				{
					SDR::Error::Make("Could not enable module \"%s\"", prop.first.c_str());
					throw;
				}
			}
		}

		ConfigObjectData* PopulateAndFindObject(rapidjson::Document& document, std::vector<ConfigObjectData>& dest)
		{
			MemberLoop(document, [&](rapidjson::Document::MemberIterator gameit)
			{
				dest.emplace_back();
				auto& curobj = dest.back();

				curobj.ObjectName = gameit->name.GetString();

				MemberLoop(gameit->value, [&](rapidjson::Document::MemberIterator gamedata)
				{
					curobj.Properties.emplace_back(gamedata->name.GetString(), std::move(gamedata->value));
				});
			});

			auto gamename = SDR::Library::GetGamePath();

			for (auto& obj : dest)
			{
				if (SDR::String::EndsWith(gamename, obj.ObjectName.c_str()))
				{
					return &obj;
				}
			}

			return nullptr;
		}

		void SetupGame()
		{
			rapidjson::Document document;

			try
			{
				document = SDR::Json::FromFile(SDR::Library::BuildResourcePath("GameConfig.json"));
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find game config"s);
			}

			auto object = PopulateAndFindObject(document, GameConfigs);

			if (!object)
			{
				SDR::Error::Make("Could not find current game in game config"s);
			}

			ResolveInherit(object, GameConfigs, document.GetAllocator());
			CallGameHandlers(object);

			GameConfigs.clear();
		}

		void SetupExtensions()
		{
			rapidjson::Document document;

			try
			{
				document = SDR::Json::FromFile(SDR::Library::BuildResourcePath("ExtensionConfig.json"));
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find extension config"s);
			}

			auto object = PopulateAndFindObject(document, ExtensionConfigs);

			if (!object)
			{
				SDR::Error::Make("Could not find current game in extension config"s);
			}

			ResolveInherit(object, ExtensionConfigs, document.GetAllocator());
			CallExtensionHandlers(object);

			ExtensionConfigs.clear();
		}
	}

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

			auto codes =
			{
				MH_EnableHook(A::ThisHook.TargetFunction),
				MH_EnableHook(ExA::ThisHook.TargetFunction),
				MH_EnableHook(W::ThisHook.TargetFunction),
				MH_EnableHook(ExW::ThisHook.TargetFunction),
			};

			for (auto code : codes)
			{
				if (code != MH_OK)
				{
					SDR::Error::Make("Could not enable library intercepts"s);
				}
			}
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
	auto res = SDR::Hooking::Initialize();

	if (res != MH_OK)
	{
		SDR::Error::Make("Could not initialize hooks"s);
	}

	LoadLibraryIntercept::Start();
}

void SDR::Setup()
{
	LoadLibraryIntercept::End();
	
	Config::SetupGame();

	for (auto entry : MainApplication.StartupFunctions)
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

		SDR::Log::Message("SDR: Passed startup procedure: \"%s\"\n", entry.Name);
	}

	MainApplication.StartupFunctions.clear();

	ExtensionManager::LoadExtensions();

	if (SDR::ExtensionManager::HasExtensions())
	{
		Config::SetupExtensions();
		ExtensionManager::Events::Ready();
	}
}

void SDR::Close()
{
	for (auto func : MainApplication.ShutdownFunctions)
	{
		func();
	}

	SDR::Hooking::Shutdown();
}

void SDR::AddStartupFunction(const StartupFuncData& data)
{
	MainApplication.StartupFunctions.emplace_back(data);
}

void SDR::AddShutdownFunction(ShutdownFuncType function)
{
	MainApplication.ShutdownFunctions.emplace_back(function);
}

void SDR::AddModuleHandler(const ModuleHandlerData& data)
{
	MainApplication.ModuleHandlers.emplace_back(data);
}
