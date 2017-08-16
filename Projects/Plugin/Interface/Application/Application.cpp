#include "PrecompiledHeader.hpp"
#include "Interface\PluginInterface.hpp"
#include "SDR Plugin API\ExportTypes.hpp"
#include "Application.hpp"

namespace
{
	struct Application
	{
		std::vector<SDR::ModuleHandlerData> ModuleHandlers;
		std::vector<SDR::StartupFuncData> StartupFunctions;
		std::vector<SDR::ShutdownFuncType> ShutdownFunctions;
	};

	Application MainApplication;

	namespace Memory
	{
		/*
			Not accessing the STL iterators in debug mode
			makes this run >10x faster, less sitting around
			waiting for nothing.
		*/
		inline bool DataCompare(const uint8_t* data, const SDR::BytePattern::Entry* pattern, size_t patternlength)
		{
			int index = 0;

			for (size_t i = 0; i < patternlength; i++)
			{
				auto byte = *pattern;

				if (!byte.Unknown && *data != byte.Value)
				{
					return false;
				}
				
				++data;
				++pattern;
				++index;
			}

			return index == patternlength;
		}

		void* FindPattern(void* start, size_t searchlength, const SDR::BytePattern& pattern)
		{
			auto patternstart = pattern.Bytes.data();
			auto length = pattern.Bytes.size();
			
			for (size_t i = 0; i <= searchlength - length; ++i)
			{
				auto addr = (const uint8_t*)(start) + i;
				
				if (DataCompare(addr, patternstart, length))
				{
					return (void*)(addr);
				}
			}

			return nullptr;
		}
	}

	namespace Config
	{
		namespace Registry
		{
			struct DataType
			{
				struct TypeIndex
				{
					enum Type
					{
						Invalid,
						UInt32,
					};
				};

				template <typename T>
				struct TypeReturn
				{
					explicit operator bool() const
					{
						return Address && Type != TypeIndex::Invalid;
					}

					template <typename T>
					void Set(T value)
					{
						if (*this)
						{
							*Address = value;
						}
					}

					T Get() const
					{
						return *Address;
					}

					bool IsUInt32() const
					{
						return Type == TypeIndex::UInt32;
					}

					TypeIndex::Type Type = TypeIndex::Invalid;
					T* Address = nullptr;
				};

				const char* Name = nullptr;
				TypeIndex::Type TypeNumber = TypeIndex::Invalid;

				union
				{
					uint32_t Value_U32;
				};

				template <typename T>
				void SetValue(T value)
				{
					if (std::is_same<T, uint32_t>::value)
					{
						TypeNumber = TypeIndex::Type::UInt32;
						Value_U32 = value;
					}
				}

				template <typename T>
				TypeReturn<T> GetActiveValue()
				{
					TypeReturn<T> ret;
					ret.Type = TypeNumber;

					switch (TypeNumber)
					{
						case TypeIndex::UInt32:
						{
							ret.Address = &Value_U32;							
							return ret;
						}
					}

					return {};
				}
			};

			std::vector<DataType> KeyValues;

			template <typename T>
			void InsertKeyValue(const char* name, T value)
			{
				DataType newtype;
				newtype.Name = name;
				newtype.SetValue(value);

				KeyValues.emplace_back(std::move(newtype));
			}
		}

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

		struct GameData
		{
			using MemberIt = rapidjson::Document::MemberIterator;
			using ValueType = rapidjson::Value;

			std::string Name;
			std::vector<std::pair<std::string, ValueType>> Properties;
		};

		std::vector<GameData> Configs;

		void ResolveInherit(GameData* targetgame, rapidjson::Document::AllocatorType& alloc)
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
						SDR::Log::Warning("SDR: %s inherit field not a string\n", targetgame->Name.c_str());
						return;
					}

					std::string from = it->second.GetString();

					targetgame->Properties.erase(it);

					for (const auto& game : Configs)
					{
						bool foundgame = false;

						if (game.Name == from)
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
							SDR::Error::Make("%s inherit target %s not found", targetgame->Name.c_str(), from.c_str());
						}
					}

					break;
				}
			}

			if (foundinherit)
			{
				ResolveInherit(targetgame, alloc);
			}
		}

		void CallHandlers(GameData* game)
		{
			SDR::Log::Message("SDR: Creating %d modules\n", MainApplication.ModuleHandlers.size());

			for (auto& prop : game->Properties)
			{
				bool found = false;

				for (auto& handler : MainApplication.ModuleHandlers)
				{
					if (prop.first == handler.Name)
					{
						found = true;

						auto res = handler.Function(handler.Name, prop.second);

						if (!res)
						{
							SDR::Error::Make("Could not enable module %s", handler.Name);
						}

						auto comment = "no comment";

						auto it = prop.second.FindMember("Comment");

						if (it != prop.second.MemberEnd())
						{
							comment = it->value.GetString();
						}

						SDR::Log::Message("SDR: Enabled module %s (%s)\n", handler.Name, comment);
						break;
					}
				}

				if (!found)
				{
					SDR::Log::Warning("SDR: No handler found for %s\n", prop.first.c_str());
				}
			}

			MainApplication.ModuleHandlers.clear();
		}

		void SetupGame(const char* gamepath, const char* gamename)
		{
			char cfgpath[1024];
		
			strcpy_s(cfgpath, gamepath);
			strcat_s(cfgpath, "SDR\\GameConfig.json");

			SDR::File::ScopedFile config;

			try
			{
				config.Assign(cfgpath, "rb");
			}

			catch (SDR::File::ScopedFile::ExceptionType status)
			{
				SDR::Error::Make("Could not find game config");
			}

			auto strdata = config.ReadString();

			rapidjson::Document document;
			document.Parse(strdata.c_str());

			GameData* currentgame = nullptr;

			MemberLoop
			(
				document, [&](GameData::MemberIt gameit)
				{
					Configs.emplace_back();
					auto& curgame = Configs.back();

					curgame.Name = gameit->name.GetString();

					MemberLoop
					(
						gameit->value, [&](GameData::MemberIt gamedata)
						{
							curgame.Properties.emplace_back(gamedata->name.GetString(), std::move(gamedata->value));
						}
					);
				}
			);

			for (auto& game : Configs)
			{
				if (game.Name == gamename)
				{
					currentgame = &game;
					break;
				}
			}

			if (!currentgame)
			{
				SDR::Error::Make("Could not find current game in game config");
			}

			ResolveInherit(currentgame, document.GetAllocator());
			CallHandlers(currentgame);
		}
	}

	namespace LoadLibraryIntercept
	{
		namespace Common
		{
			template <typename T>
			using TableType = std::initializer_list<std::pair<const T*, std::function<void()>>>;

			void Load(HMODULE module, const char* name)
			{
				TableType<char> table =
				{
					
				};

				for (auto& entry : table)
				{
					if (SDR::String::EndsWith(name, entry.first))
					{
						entry.second();
						break;
					}
				}
			}

			void Load(HMODULE module, const wchar_t* name)
			{
				TableType<wchar_t> table =
				{
					
				};

				for (auto& entry : table)
				{
					if (SDR::String::EndsWith(name, entry.first))
					{
						entry.second();
						break;
					}
				}
			}
		}

		namespace A
		{
			SDR::HookModule<decltype(LoadLibraryA)*> ThisHook;

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
			SDR::HookModule<decltype(LoadLibraryExA)*> ThisHook;

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
			SDR::HookModule<decltype(LoadLibraryW)*> ThisHook;

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
			SDR::HookModule<decltype(LoadLibraryExW)*> ThisHook;

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
			auto results =
			{
				SDR::CreateHookAPI(L"kernel32.dll", "LoadLibraryA", A::ThisHook, A::Override),
				SDR::CreateHookAPI(L"kernel32.dll", "LoadLibraryExA", ExA::ThisHook, ExA::Override),
				SDR::CreateHookAPI(L"kernel32.dll", "LoadLibraryW", W::ThisHook, W::Override),
				SDR::CreateHookAPI(L"kernel32.dll", "LoadLibraryExW", ExW::ThisHook, ExW::Override),
			};

			for (auto res : results)
			{
				if (!res)
				{
					throw SDR::API::InitializeCode::CouldNotCreateLibraryIntercepts;
				}
			}

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
					throw SDR::API::InitializeCode::CouldNotEnableLibraryIntercepts;
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
	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		throw SDR::API::InitializeCode::CouldNotInitializeHooks;
	}

	LoadLibraryIntercept::Start();
}

void SDR::Setup(const char* gamepath, const char* gamename)
{
	LoadLibraryIntercept::End();
	Config::SetupGame(gamepath, gamename);
}

void SDR::Close()
{
	for (auto func : MainApplication.ShutdownFunctions)
	{
		func();
	}

	MH_Uninitialize();
}

void SDR::AddPluginStartupFunction(const StartupFuncData& data)
{
	MainApplication.StartupFunctions.emplace_back(data);
}

void SDR::CallPluginStartupFunctions()
{
	if (MainApplication.StartupFunctions.empty())
	{
		return;
	}

	auto count = MainApplication.StartupFunctions.size();
	auto index = 0;

	for (auto entry : MainApplication.StartupFunctions)
	{
		SDR::Log::Message("SDR: Startup procedure (%d/%d): %s\n", index + 1, count, entry.Name);

		auto res = entry.Function();

		if (!res)
		{
			SDR::Error::Make("Startup procedure %s failed"s);
		}

		++index;
	}

	MainApplication.StartupFunctions.clear();
}

void SDR::AddPluginShutdownFunction(ShutdownFuncType function)
{
	MainApplication.ShutdownFunctions.emplace_back(function);
}

void SDR::AddModuleHandler(const ModuleHandlerData& data)
{
	MainApplication.ModuleHandlers.emplace_back(data);
}

SDR::BytePattern SDR::GetPatternFromString(const char* input)
{
	BytePattern ret;

	while (*input)
	{
		if (std::isspace(*input))
		{
			++input;
		}

		BytePattern::Entry entry;

		if (std::isxdigit(*input))
		{
			entry.Unknown = false;
			entry.Value = std::strtol(input, nullptr, 16);

			input += 2;
		}

		else
		{
			entry.Unknown = true;
			input += 2;
		}

		ret.Bytes.emplace_back(entry);
	}

	return ret;
}

void* SDR::GetAddressFromPattern(const ModuleInformation& library, const BytePattern& pattern)
{
	return Memory::FindPattern(library.MemoryBase, library.MemorySize, pattern);
}

bool SDR::JsonHasPattern(rapidjson::Value& value)
{
	if (value.HasMember("Pattern"))
	{
		return true;
	}

	return false;
}

bool SDR::JsonHasVirtualIndexOnly(rapidjson::Value& value)
{
	if (value.HasMember("VTIndex"))
	{
		return true;
	}

	return false;
}

bool SDR::JsonHasVirtualIndexAndNamePtr(rapidjson::Value& value)
{
	if (JsonHasVirtualIndexOnly(value))
	{
		if (value.HasMember("VTPtrName"))
		{
			return true;
		}
	}

	return false;
}

bool SDR::JsonHasVariant(rapidjson::Value& value)
{
	if (value.HasMember("Variant"))
	{
		return true;
	}

	return false;
}

void* SDR::GetAddressFromJsonFlex(rapidjson::Value& value)
{
	if (JsonHasPattern(value))
	{
		return GetAddressFromJsonPattern(value);
	}

	else if (JsonHasVirtualIndexAndNamePtr(value))
	{
		return GetVirtualAddressFromJson(value);
	}

	return nullptr;
}

void* SDR::GetAddressFromJsonPattern(rapidjson::Value& value)
{
	auto module = value["Module"].GetString();
	auto patternstr = value["Pattern"].GetString();

	int offset = 0;
	bool isjump = false;

	{
		auto iter = value.FindMember("Offset");

		if (iter != value.MemberEnd())
		{
			offset = iter->value.GetInt();
		}
	}

	if (value.HasMember("IsRelativeJump"))
	{
		isjump = true;
	}

	auto pattern = GetPatternFromString(patternstr);

	AddressFinder address(module, pattern, offset);

	if (isjump)
	{
		RelativeJumpFunctionFinder jumper(address.Get());
		return jumper.Get();
	}

	return address.Get();
}

int SDR::GetVariantFromJson(rapidjson::Value& value)
{
	if (JsonHasVariant(value))
	{
		return value["Variant"].GetInt();
	}

	return 0;
}

void SDR::WarnAboutHookVariant(const char* name, int variant)
{
	SDR::Log::Warning("SDR: No such hook overload for %s in variant %d\n", variant);
}

bool SDR::WarnIfVariantOutOfBounds(const char* name, int variant, int max)
{
	if (variant < 0 || variant > max)
	{
		SDR::Log::Warning("SDR: Variant overload %d for %s not in bounds (%d max)\n", variant, max);
		return true;
	}

	return false;
}

void* SDR::GetVirtualAddressFromIndex(void* ptr, int index)
{
	auto vtable = *((void***)ptr);
	auto address = vtable[index];

	return address;
}

void* SDR::GetVirtualAddressFromJson(void* ptr, rapidjson::Value& value)
{
	auto index = GetVirtualIndexFromJson(value);
	return GetVirtualAddressFromIndex(ptr, index);
}

int SDR::GetVirtualIndexFromJson(rapidjson::Value& value)
{
	return value["VTIndex"].GetInt();
}

void* SDR::GetVirtualAddressFromJson(rapidjson::Value& value)
{
	auto instance = value["VTPtrName"].GetString();

	uint32_t ptrnum;
	auto res = ModuleShared::Registry::GetKeyValue(instance, &ptrnum);

	if (!res)
	{
		return nullptr;
	}

	auto ptr = (void*)ptrnum;

	return GetVirtualAddressFromJson(ptr, value);
}

void SDR::ModuleShared::Registry::SetKeyValue(const char* name, uint32_t value)
{
	Config::Registry::InsertKeyValue(name, value);
}

bool SDR::ModuleShared::Registry::GetKeyValue(const char* name, uint32_t* value)
{
	*value = 0;

	for (auto& keyvalue : Config::Registry::KeyValues)
	{
		if (strcmp(keyvalue.Name, name) == 0)
		{
			auto active = keyvalue.GetActiveValue<uint32_t>();

			if (active && value)
			{
				*value = active.Get();
			}

			return true;
		}
	}

	return false;
}

bool SDR::GenericVariantInit(ModuleShared::Variant::Entry& entry, const char* name, rapidjson::Value& value, int maxvariant)
{
	auto addr = SDR::GetAddressFromJsonFlex(value);
	auto variant = SDR::GetVariantFromJson(value);

	if (WarnIfVariantOutOfBounds(name, variant, maxvariant))
	{
		return false;
	}

	return ModuleShared::SetFromAddress(entry, addr, variant);
}

bool SDR::CreateHookBare(HookModuleBare& hook, void* override, void* address)
{
	hook.TargetFunction = address;
	hook.NewFunction = override;

	auto res = MH_CreateHookEx(hook.TargetFunction, hook.NewFunction, &hook.OriginalFunction);

	if (res != MH_OK)
	{
		return false;
	}

	res = MH_EnableHook(hook.TargetFunction);

	if (res != MH_OK)
	{
		return false;
	}

	return true;
}

bool SDR::CreateHookBareShort(HookModuleBare& hook, void* override, rapidjson::Value& value)
{
	auto address = GetAddressFromJsonPattern(value);
	return CreateHookBare(hook, override, address);
}

bool SDR::CreateHookAPI(const wchar_t* module, const char* name, HookModuleBare& hook, void* override)
{
	hook.NewFunction = override;
	
	auto res = MH_CreateHookApiEx(module, name, override, &hook.OriginalFunction, &hook.TargetFunction);

	return res == MH_OK;
}

bool SDR::GenericHookVariantInit(std::initializer_list<GenericHookInitParam>&& hooks, const char* name, rapidjson::Value& value)
{
	auto variant = SDR::GetVariantFromJson(value);
	auto size = hooks.size();

	if (WarnIfVariantOutOfBounds(name, variant, size))
	{
		return false;
	}

	auto target = *(hooks.begin() + variant);

	return CreateHookBareShort(target.Hook, target.Override, value);
}
