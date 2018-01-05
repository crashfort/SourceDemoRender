#include <SDR Shared\Hooking.hpp>
#include <SDR Shared\BareWindows.hpp>
#include <SDR Shared\Json.hpp>
#include <cctype>
#include <cstdint>
#include <Psapi.h>

namespace
{
	namespace LocalRegistry
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

	namespace Memory
	{
		/*
			Not accessing the STL iterators in debug mode makes this run >10x faster, less sitting around waiting for nothing.
		*/
		inline bool DataCompare(const uint8_t* data, const SDR::Hooking::BytePattern::Entry* pattern, size_t patternlength)
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

		void* FindPattern(void* start, size_t searchlength, const SDR::Hooking::BytePattern& pattern)
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
}

#ifndef SDR_HOOKING_NO_MH

MH_STATUS SDR::Hooking::Initialize()
{
	return MH_Initialize();
}

void SDR::Hooking::Shutdown()
{
	MH_Uninitialize();
}

#endif

SDR::Hooking::ModuleInformation::ModuleInformation(const char* name) : Name(name)
{
	SDR::Error::ScopedContext e1("ModuleInformation"s);

	MODULEINFO info;

	SDR::Error::Microsoft::ThrowIfZero
	(
		K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(name), &info, sizeof(info)),
		"Could not get module information for \"%s\"", name
	);

	MemoryBase = info.lpBaseOfDll;
	MemorySize = info.SizeOfImage;
}

SDR::Hooking::BytePattern SDR::Hooking::GetPatternFromString(const char* input)
{
	BytePattern ret;

	bool shouldbespace = false;

	while (*input)
	{
		if (SDR::String::IsSpace(*input))
		{
			++input;
			shouldbespace = false;
		}

		else if (shouldbespace)
		{
			SDR::Error::Make("Error in string byte pair formatting"s);
		}

		BytePattern::Entry entry;

		if (std::isxdigit(*input))
		{
			entry.Unknown = false;
			entry.Value = std::strtol(input, nullptr, 16);

			input += 2;

			shouldbespace = true;
		}

		else
		{
			entry.Unknown = true;
			input += 2;

			shouldbespace = true;
		}

		ret.Bytes.emplace_back(entry);
	}

	if (ret.Bytes.empty())
	{
		SDR::Error::Make("Empty byte pattern"s);
	}

	return ret;
}

void* SDR::Hooking::GetAddressFromPattern(const ModuleInformation& library, const BytePattern& pattern)
{
	return Memory::FindPattern(library.MemoryBase, library.MemorySize, pattern);
}

bool SDR::Hooking::JsonHasPattern(const rapidjson::Value& value)
{
	if (value.HasMember("Pattern"))
	{
		return true;
	}

	return false;
}

bool SDR::Hooking::JsonHasVirtualIndexOnly(const rapidjson::Value& value)
{
	if (value.HasMember("VTIndex"))
	{
		return true;
	}

	return false;
}

bool SDR::Hooking::JsonHasVirtualIndexAndNamePtr(const rapidjson::Value& value)
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

bool SDR::Hooking::JsonHasVariant(const rapidjson::Value& value)
{
	if (value.HasMember("Variant"))
	{
		return true;
	}

	return false;
}

void* SDR::Hooking::GetAddressFromJsonFlex(const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GetAddressFromJsonFlex"s);

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

void* SDR::Hooking::GetAddressFromJsonPattern(const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GetAddressFromJsonPattern"s);

	auto module = SDR::Json::GetString(value, "Module");
	auto patternstr = SDR::Json::GetString(value, "Pattern");

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
	SDR::Error::ThrowIfNull(address.Get());

	if (isjump)
	{
		SDR::Error::ScopedContext e2("Jump"s);

		RelativeJumpFunctionFinder jumper(address.Get());
		SDR::Error::ThrowIfNull(jumper.Get());

		return jumper.Get();
	}

	return address.Get();
}

int SDR::Hooking::GetVariantFromJson(const rapidjson::Value& value, int max)
{
	SDR::Error::ScopedContext e1("GetVariantFromJson"s);

	if (JsonHasVariant(value))
	{
		auto ret = SDR::Json::GetInt(value, "Variant");

		WarnIfVariantOutOfBounds(ret, max);

		return ret;
	}

	return 0;
}

void SDR::Hooking::WarnIfVariantOutOfBounds(int variant, int max)
{
	SDR::Error::ScopedContext e1("WarnIfVariantOutOfBounds"s);

	if (variant < 0 || variant >= max)
	{
		SDR::Error::Make("Variant overload %d not in bounds (%d max)", variant, max - 1);
	}
}

void* SDR::Hooking::GetVirtualAddressFromIndex(void* ptr, int index)
{
	auto vtable = *((void***)ptr);
	auto address = vtable[index];

	return address;
}

void* SDR::Hooking::GetVirtualAddressFromJson(void* ptr, const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GetVirtualAddressFromJson"s);

	auto index = GetVirtualIndexFromJson(value);
	return GetVirtualAddressFromIndex(ptr, index);
}

int SDR::Hooking::GetVirtualIndexFromJson(const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GetVirtualIndexFromJson"s);

	return SDR::Json::GetInt(value, "VTIndex");
}

void* SDR::Hooking::GetVirtualAddressFromJson(const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GetVirtualAddressFromJson"s);

	auto instance = SDR::Json::GetString(value, "VTPtrName");

	uint32_t ptrnum;
	auto res = ModuleShared::Registry::GetKeyValue(instance, &ptrnum);

	if (!res)
	{
		SDR::Error::Make("Could not find virtual object name \"%s\"", instance);
	}

	auto ptr = (void*)ptrnum;
	SDR::Error::ThrowIfNull(ptr, "Registry value \"%s\" was null", instance);

	return GetVirtualAddressFromJson(ptr, value);
}

void SDR::Hooking::ModuleShared::Registry::SetKeyValue(const char* name, uint32_t value)
{
	LocalRegistry::InsertKeyValue(name, value);
}

bool SDR::Hooking::ModuleShared::Registry::GetKeyValue(const char* name, uint32_t* value)
{
	*value = 0;

	for (auto& keyvalue : LocalRegistry::KeyValues)
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

void SDR::Hooking::GenericVariantInit(ModuleShared::Variant::Entry& entry, const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GenericVariantInit"s);

	auto addr = GetAddressFromJsonFlex(value);
	auto variant = GetVariantFromJson(value, entry.VariantCount);

	ModuleShared::SetFromAddress(entry, addr, variant);
}

#ifndef SDR_HOOKING_NO_MH

void SDR::Hooking::CreateHookBare(HookModuleBare& hook, void* override, void* address)
{
	SDR::Error::ScopedContext e1("CreateHookBare"s);

	hook.TargetFunction = address;
	hook.NewFunction = override;

	auto res = MH_CreateHookEx(hook.TargetFunction, hook.NewFunction, &hook.OriginalFunction);

	if (res != MH_OK)
	{
		SDR::Error::Make("Could not create hook (\"%s\")", MH_StatusToString(res));
	}

	res = MH_EnableHook(hook.TargetFunction);

	if (res != MH_OK)
	{
		SDR::Error::Make("Could not enable hook (\"%s\")", MH_StatusToString(res));
	}
}

void SDR::Hooking::CreateHookBareShort(HookModuleBare& hook, void* override, const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("CreateHookBareShort"s);

	auto address = GetAddressFromJsonPattern(value);
	CreateHookBare(hook, override, address);
}

void SDR::Hooking::CreateHookAPI(const wchar_t* module, const char* name, HookModuleBare& hook, void* override)
{
	SDR::Error::ScopedContext e1("CreateHookAPI"s);

	hook.NewFunction = override;
	
	auto res = MH_CreateHookApiEx(module, name, override, &hook.OriginalFunction, &hook.TargetFunction);

	if (res != MH_OK)
	{
		SDR::Error::Make("Could not create API hook \"%s\" (\"%s\")", name, MH_StatusToString(res));
	}
}

int SDR::Hooking::GenericHookVariantInit(std::initializer_list<GenericHookInitParam> hooks, const rapidjson::Value& value)
{
	SDR::Error::ScopedContext e1("GenericHookVariantInit"s);

	auto size = hooks.size();
	auto variant = GetVariantFromJson(value, size);

	auto target = *(hooks.begin() + variant);

	CreateHookBareShort(target.Hook, target.Override, value);

	return variant;
}

#endif