#pragma once
#include <array>
#include "SDR Shared\Json.hpp"

namespace SDR
{
	void PreEngineSetup();

	void Setup(const char* gamepath, const char* gamename);
	void Close();

	struct StartupFuncData
	{
		using FuncType = void(*)();

		const char* Name;
		FuncType Function;
	};

	void AddPluginStartupFunction(const StartupFuncData& data);

	struct PluginStartupFunctionAdder
	{
		PluginStartupFunctionAdder(const char* name, StartupFuncData::FuncType function)
		{
			StartupFuncData data;
			data.Name = name;
			data.Function = function;

			AddPluginStartupFunction(data);
		}
	};

	using ShutdownFuncType = void(*)();
	void AddPluginShutdownFunction(ShutdownFuncType function);

	struct PluginShutdownFunctionAdder
	{
		PluginShutdownFunctionAdder(ShutdownFuncType function)
		{
			AddPluginShutdownFunction(function);
		}
	};

	struct ModuleHandlerData
	{
		using FuncType = bool(*)
		(
			const char* name,
			rapidjson::Value& value
		);

		const char* Name;
		FuncType Function;
	};

	void AddModuleHandler(const ModuleHandlerData& data);

	struct ModuleHandlerAdder
	{
		ModuleHandlerAdder(const char* name, ModuleHandlerData::FuncType function)
		{
			ModuleHandlerData data;
			data.Name = name;
			data.Function = function;

			AddModuleHandler(data);
		}
	};

	template <typename... Types>
	constexpr auto CreateAdders(Types&&... types)
	{
		return std::array<std::common_type_t<Types...>, sizeof...(Types)>{{std::forward<Types>(types)...}};
	}

	struct ModuleInformation
	{
		ModuleInformation(const char* name) : Name(name)
		{
			MODULEINFO info;
			
			K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(name), &info, sizeof(info));

			MemoryBase = info.lpBaseOfDll;
			MemorySize = info.SizeOfImage;
		}

		const char* Name;

		void* MemoryBase;
		size_t MemorySize;
	};

	struct BytePattern
	{
		struct Entry
		{
			uint8_t Value;
			bool Unknown;
		};

		std::vector<Entry> Bytes;
	};

	struct HookModuleBare
	{
		void* TargetFunction;
		void* NewFunction;
		void* OriginalFunction;
	};

	template <typename FuncSignature>
	struct HookModule : HookModuleBare
	{
		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}
	};

	BytePattern GetPatternFromString(const char* input);

	void* GetAddressFromPattern(const ModuleInformation& library, const BytePattern& pattern);

	bool JsonHasPattern(rapidjson::Value& value);
	bool JsonHasVirtualIndexOnly(rapidjson::Value& value);
	bool JsonHasVirtualIndexAndNamePtr(rapidjson::Value& value);
	bool JsonHasVariant(rapidjson::Value& value);

	void* GetAddressFromJsonFlex(rapidjson::Value& value);
	void* GetAddressFromJsonPattern(rapidjson::Value& value);

	int GetVariantFromJson(rapidjson::Value& value);

	void WarnAboutHookVariant(const char* name, int variant);
	bool WarnIfVariantOutOfBounds(const char* name, int variant, int max);

	void* GetVirtualAddressFromIndex(void* ptr, int index);

	template <typename T>
	struct VirtualIndex
	{
		using FuncType = T;

		void operator=(int other)
		{
			Index = other;
		}

		int Index;
	};

	template <typename FuncType>
	auto GetVirtual(void* ptr, VirtualIndex<FuncType>& index)
	{
		auto address = GetVirtualAddressFromIndex(ptr, index.Index);

		auto func = (FuncType)(address);
		return func;
	}

	void* GetVirtualAddressFromJson(void* ptr, rapidjson::Value& value);

	int GetVirtualIndexFromJson(rapidjson::Value& value);

	void* GetVirtualAddressFromJson(rapidjson::Value& value);

	namespace ModuleShared
	{
		namespace Registry
		{
			void SetKeyValue(const char* name, uint32_t value);
			bool GetKeyValue(const char* name, uint32_t* value);

			inline void SetKeyValue(const char* name, void* value)
			{
				SetKeyValue(name, (uintptr_t)value);
			}
		}

		namespace Variant
		{
			struct Entry
			{
				void* Address;
				int Variant;

				bool operator==(int other) const
				{
					return Variant == other;
				}
			};

			template <typename T>
			struct Function
			{
				using Type = T;

				Function(Entry& entry) : TargetEntry(entry)
				{

				}

				auto operator()() const
				{
					return (Type)TargetEntry.Address;
				}

				Entry& TargetEntry;
			};

			struct HookFunction
			{
				HookFunction(Entry& entry) : TargetEntry(entry)
				{

				}

				Entry& TargetEntry;
			};
		}

		template <typename T>
		inline bool SetFromAddress(T& obj, void* address)
		{
			obj = (T)address;

			if (!address)
			{
				return false;
			}

			return true;
		}

		inline bool SetFromAddress(Variant::Entry& entry, void* address, int variant)
		{
			entry.Address = address;
			entry.Variant = variant;

			if (!address)
			{
				return false;
			}

			return true;
		}
	}

	bool GenericVariantInit(ModuleShared::Variant::Entry& entry, const char* name, rapidjson::Value& value, int maxvariant);

	bool CreateHookBare(HookModuleBare& hook, void* override, void* address);

	template <typename FuncType>
	bool CreateHook(HookModule<FuncType>& hook, FuncType override, void* address)
	{
		return CreateHookBare(hook, override, address);
	}

	template <typename FuncType>
	bool CreateHook(HookModule<FuncType>& hook, FuncType override, const char* module, const BytePattern& pattern)
	{
		auto address = GetAddressFromPattern(module, pattern);
		return CreateHook(hook, override, address);
	}

	template <typename FuncType>
	bool CreateHookShort(HookModule<FuncType>& hook, FuncType override, rapidjson::Value& value)
	{
		auto address = GetAddressFromJsonPattern(value);
		return CreateHook(hook, override, address);
	}

	bool CreateHookBareShort(HookModuleBare& hook, void* override, rapidjson::Value& value);

	bool CreateHookAPI(const wchar_t* module, const char* name, HookModuleBare& hook, void* override);

	struct GenericHookInitParam
	{
		GenericHookInitParam(HookModuleBare& hook, void* override) : Hook(hook), Override(override)
		{

		}

		HookModuleBare& Hook;
		void* Override;
	};

	bool GenericHookVariantInit(std::initializer_list<GenericHookInitParam>&& hooks, const char* name, rapidjson::Value& value);

	struct AddressFinder
	{
		AddressFinder(const char* module, const BytePattern& pattern, int offset = 0)
		{
			auto addr = GetAddressFromPattern(module, pattern);

			auto addrmod = static_cast<uint8_t*>(addr);

			if (addrmod)
			{
				/*
					Increment for any extra instructions.
				*/
				addrmod += offset;
			}

			Address = addrmod;
		}

		void* Get() const
		{
			return Address;
		}

		void* Address;
	};

	/*
		First byte at target address should be E8.
	*/
	struct RelativeJumpFunctionFinder
	{
		RelativeJumpFunctionFinder(void* address)
		{
			auto addrmod = static_cast<uint8_t*>(address);

			/*
				Skip the E8 byte.
			*/
			addrmod += sizeof(uint8_t);

			auto offset = *reinterpret_cast<ptrdiff_t*>(addrmod);

			/*
				E8 jumps count relative distance from the next instruction,
				in 32 bit the distance will be measued in 4 bytes.
			*/
			addrmod += sizeof(uintptr_t);

			/*
				Do the jump, address will now be the target function.
			*/
			addrmod += offset;

			Address = addrmod;
		}

		void* Get() const
		{
			return Address;
		}

		void* Address;
	};

	struct StructureWalker
	{
		StructureWalker(void* address) :
			Address(static_cast<uint8_t*>(address)),
			Start(Address)
		{

		}

		template <typename Modifier = uint8_t>
		uint8_t* Advance(int offset)
		{
			Address += offset * sizeof(Modifier);
			return Address;
		}

		template <typename Modifier = uint8_t>
		uint8_t* AdvanceAbsolute(int offset)
		{
			Reset();
			Address += offset * sizeof(Modifier);
			return Address;
		}

		void Reset()
		{
			Address = Start;
		}

		uint8_t* Address;
		uint8_t* Start;
	};
}
