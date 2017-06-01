#include "PrecompiledHeader.hpp"
#include "Application.hpp"
#include "dbg.h"

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
		inline bool DataCompare
		(
			const uint8_t* data,
			const SDR::BytePattern::Entry* pattern,
			size_t patternlength
		)
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

		void* FindPattern
		(
			void* start,
			size_t searchlength,
			const SDR::BytePattern& pattern
		)
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
		enum class Status
		{
			CouldNotFindConfig,
			CouldNotFindGame,

			InheritTargetWrong,
			HandlerNotFound,

			CouldNotCreateModule,
		};

		const char* StatusNames[] =
		{
			"Could not find config",
			"Could not find game",
			"Inherit target not found",
			"Module handler not found",
			"Could not create module",
		};

		template
		<
			typename NodeType
		>
		auto SafeFindMember
		(
			NodeType& node,
			const char* name,
			Status code
		)
		{
			auto it = node.FindMember(name);

			if (it == node.MemberEnd())
			{
				throw code;
			}

			return it;
		}

		template
		<
			typename NodeType,
			typename FuncType
		>
		void MemberLoop
		(
			NodeType& node,
			FuncType callback
		)
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

		void ResolveInherit
		(
			GameData* targetgame,
			rapidjson::Document::AllocatorType& alloc
		)
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
						Warning
						(
							"SDR: %s inherit field not a string\n",
							targetgame->Name.c_str()
						);

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
									targetgame->Properties.emplace_back
									(
										sourceprop.first,
										rapidjson::Value
										(
											sourceprop.second,
											alloc
										)
									);
								}
							}

							break;
						}

						if (!foundgame)
						{
							Warning
							(
								"SDR: %s inherit target %s not found\n",
								targetgame->Name.c_str(),
								from.c_str()
							);

							throw Status::InheritTargetWrong;
						}
					}

					break;
				}
			}

			if (foundinherit)
			{
				ResolveInherit
				(
					targetgame,
					alloc
				);
			}
		}

		void CallHandlers
		(
			GameData* game
		)
		{
			Msg
			(
				"SDR: Creating %d modules\n",
				MainApplication.ModuleHandlers.size()
			);

			for (auto& prop : game->Properties)
			{
				bool foundhadler = false;

				for (auto& handler : MainApplication.ModuleHandlers)
				{
					if (prop.first == handler.Name)
					{
						foundhadler = true;

						auto res = handler.Function
						(
							prop.second
						);

						if (!res)
						{
							Warning
							(
								"SDR: Could not enable module %s\n",
								handler.Name
							);

							throw Status::CouldNotCreateModule;
						}

						Msg
						(
							"SDR: Enabled module %s\n",
							handler.Name
						);

						break;
					}
				}

				if (!foundhadler)
				{
					Warning
					(
						"SDR: No handler found for %s\n",
						prop.first.c_str()
					);
				}
			}
		}

		void SetupGame
		(
			const char* gamepath,
			const char* gamename
		)
		{
			char cfgpath[1024];
		
			strcpy_s(cfgpath, gamepath);
			strcat_s(cfgpath, "SDR\\GameConfig.json");

			SDR::Shared::ScopedFile config;

			try
			{
				config.Assign
				(
					cfgpath,
					"r"
				);
			}

			catch (SDR::Shared::ScopedFile::ExceptionType status)
			{
				throw Status::CouldNotFindConfig;
			}

			auto data = config.ReadAll();
			auto strdata = reinterpret_cast<const char*>(data.data());

			rapidjson::Document document;
			document.Parse(strdata, data.size());

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
							curgame.Properties.emplace_back
							(
								std::make_pair
								(
									gamedata->name.GetString(),
									std::move(gamedata->value)
								)
							);
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
				throw Status::CouldNotFindGame;
			}

			ResolveInherit
			(
				currentgame,
				document.GetAllocator()
			);

			CallHandlers
			(
				currentgame
			);
		}
	}
}

void SDR::Setup
(
	const char* gamepath,
	const char* gamename
)
{
	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		Warning
		(
			"SDR: Failed to initialize hooks\n"
		);

		throw false;
	}

	try
	{
		Config::SetupGame
		(
			gamepath,
			gamename
		);
	}

	catch (Config::Status status)
	{
		auto index = static_cast<int>(status);
		auto name = Config::StatusNames[index];

		Warning
		(
			"SDR: GameConfig: %s\n",
			name
		);

		throw false;
	}
}

void SDR::Close()
{
	for (auto func : MainApplication.ShutdownFunctions)
	{
		func();
	}

	MH_Uninitialize();
}

void SDR::AddPluginStartupFunction
(
	const StartupFuncData& data
)
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
		Msg
		(
			"SDR: Startup procedure (%d/%d): %s\n",
			index + 1,
			count,
			entry.Name
		);

		auto res = entry.Function();

		if (!res)
		{
			throw entry.Name;
		}

		++index;
	}
}

void SDR::AddPluginShutdownFunction
(
	ShutdownFuncType function
)
{
	MainApplication.ShutdownFunctions.emplace_back(function);
}

void SDR::AddModuleHandler
(
	const ModuleHandlerData& data
)
{
	MainApplication.ModuleHandlers.emplace_back(data);
}

SDR::BytePattern SDR::GetPatternFromString
(
	const char* input
)
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

void* SDR::GetAddressFromPattern
(
	const ModuleInformation& library,
	const BytePattern& pattern
)
{
	return Memory::FindPattern
	(
		library.MemoryBase,
		library.MemorySize,
		pattern
	);
}

void* SDR::GetAddressFromJsonPattern(rapidjson::Value& value)
{
	auto module = value["Module"].GetString();
	auto patternstr = value["Pattern"].GetString();
	int offset = 0;
	
	{
		auto iter = value.FindMember("Offset");

		if (iter != value.MemberEnd())
		{
			if (!iter->value.IsNumber())
			{
				Warning
				(
					"SDR: Offset field not a number\n"
				);

				return nullptr;
			}

			offset = iter->value.GetInt();
		}
	}

	auto pattern = GetPatternFromString(patternstr);

	SDR::AddressFinder address
	(
		module,
		pattern,
		offset
	);

	return address.Get();
}

void* SDR::GetVirtualAddressFromIndex
(
	void* ptr,
	int index
)
{
	auto vtable = *((void***)ptr);
	auto address = vtable[index];

	return address;
}

void* SDR::GetVirtualAddressFromJson
(
	void* ptr,
	rapidjson::Value& value
)
{
	auto offset = value["Offset"].GetInt();

	return GetVirtualAddressFromIndex
	(
		ptr,
		offset
	);
}
