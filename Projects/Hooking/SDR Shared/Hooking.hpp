#pragma once
#include <cstdint>
#include <vector>
#include <rapidjson\document.h>
#include <SDR Shared\Error.hpp>

#ifndef SDR_HOOKING_NO_MH

#include <MinHook.h>

template <typename T>
inline MH_STATUS MH_CreateHookEx(LPVOID pTarget, LPVOID pDetour, T* ppOriginal)
{
	return MH_CreateHook(pTarget, pDetour, reinterpret_cast<LPVOID*>(ppOriginal));
}

template <typename T>
inline MH_STATUS MH_CreateHookApi2(LPCWSTR pszModule, LPCSTR pszProcName, LPVOID pDetour, T* ppOriginal)
{
	return MH_CreateHookApi(pszModule, pszProcName, pDetour, reinterpret_cast<LPVOID*>(ppOriginal));
}

namespace SDR::Error::MH
{
	/*
		For use with unrecoverable errors.
	*/
	template <typename... Args>
	inline void ThrowIfFailed(MH_STATUS status, const char* format, Args&&... args)
	{
		if (status != MH_OK)
		{
			auto message = MH_StatusToString(status);

			auto user = String::Format(format, std::forward<Args>(args)...);
			auto final = String::Format("%d (%s) -> ", status, message);

			final += user;

			Make(std::move(final));
		}
	}
}

#endif

namespace SDR::Hooking
{
	#ifndef SDR_HOOKING_NO_MH
	
	MH_STATUS Initialize();
	void Shutdown();
	
	#endif

	struct ModuleInformation
	{
		ModuleInformation(const char* name);

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

	#ifndef SDR_HOOKING_NO_MH

	struct HookModuleBare
	{
		MH_STATUS Enable() const
		{
			return MH_EnableHook(TargetFunction);
		}

		MH_STATUS Disable() const
		{
			return MH_DisableHook(TargetFunction);
		}

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

	#endif

	BytePattern GetPatternFromString(const char* input);

	void* GetAddressFromPattern(const ModuleInformation& library, const BytePattern& pattern);

	bool JsonHasPattern(const rapidjson::Value& value);
	bool JsonHasVirtualIndexOnly(const rapidjson::Value& value);
	bool JsonHasVirtualIndexAndNamePtr(const rapidjson::Value& value);
	bool JsonHasVariant(const rapidjson::Value& value);

	void* GetAddressFromJsonFlex(const rapidjson::Value& value);
	void* GetAddressFromJsonPattern(const rapidjson::Value& value);

	int GetVariantFromJson(const rapidjson::Value& value, int max);

	template <typename T, typename... Rest>
	int GetVariantFromJson(const rapidjson::Value& value)
	{
		return GetVariantFromJson(value, sizeof...(Rest) + 1);
	}

	void WarnIfVariantOutOfBounds(int variant, int max);

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

	void* GetVirtualAddressFromJson(void* ptr, const rapidjson::Value& value);

	int GetVirtualIndexFromJson(const rapidjson::Value& value);

	void* GetVirtualAddressFromJson(const rapidjson::Value& value);

	namespace ModuleShared
	{
		namespace Registry
		{
			void SetKeyValue(const char* name, uint32_t value);
			bool GetKeyValue(const char* name, uint32_t* value);

			inline void SetKeyValue(const char* name, void* value)
			{
				SDR::Error::ThrowIfNull(value, "Null pointer keyvalue \"%s\"", name);
				SetKeyValue(name, (uintptr_t)value);
			}
		}

		namespace Variant
		{
			struct Entry
			{
				bool operator==(int other) const
				{
					return Variant == other;
				}

				void Visit()
				{
					++VariantCount;
				}

				void* Address;
				int VariantCount = 0;
				int Variant;
			};

			template <typename T>
			struct Function
			{
				using Type = T;

				Function(Entry& entry) : TargetEntry(entry)
				{
					entry.Visit();
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
					entry.Visit();
				}

				Entry& TargetEntry;
			};
		}

		template <typename T>
		inline void SetFromAddress(T& obj, void* address)
		{
			SDR::Error::ScopedContext e1("SetFromAddress1"s);

			SDR::Error::ThrowIfNull(address);
			
			obj = (T)address;
		}

		inline void SetFromAddress(Variant::Entry& entry, void* address, int variant)
		{
			SDR::Error::ScopedContext e1("SetFromAddress2"s);

			SDR::Error::ThrowIfNull(address);

			entry.Address = address;
			entry.Variant = variant;
		}
	}

	void GenericVariantInit(ModuleShared::Variant::Entry& entry, const rapidjson::Value& value);

	#ifndef SDR_HOOKING_NO_MH

	void CreateHookBare(HookModuleBare& hook, void* override, void* address);

	template <typename FuncType>
	void CreateHook(HookModule<FuncType>& hook, FuncType override, void* address)
	{
		CreateHookBare(hook, override, address);
	}

	template <typename FuncType>
	void CreateHook(HookModule<FuncType>& hook, FuncType override, const char* module, const BytePattern& pattern)
	{
		auto address = GetAddressFromPattern(module, pattern);
		CreateHook(hook, override, address);
	}

	template <typename FuncType>
	void CreateHookShort(HookModule<FuncType>& hook, FuncType override, const rapidjson::Value& value)
	{
		auto address = GetAddressFromJsonPattern(value);
		CreateHook(hook, override, address);
	}

	void CreateHookBareShort(HookModuleBare& hook, void* override, const rapidjson::Value& value);

	void CreateHookAPI(const wchar_t* module, const char* name, HookModuleBare& hook, void* override);

	struct GenericHookInitParam
	{
		GenericHookInitParam(HookModuleBare& hook, void* override) : Hook(hook), Override(override)
		{

		}

		HookModuleBare& Hook;
		void* Override;
	};

	int GenericHookVariantInit(std::initializer_list<GenericHookInitParam> hooks, const rapidjson::Value& value);

	#endif

	struct AddressFinder
	{
		AddressFinder(const char* module, const BytePattern& pattern, int offset = 0)
		{
			SDR::Error::ScopedContext e1("AddressFinder"s);

			auto addr = GetAddressFromPattern(module, pattern);

			SDR::Error::ThrowIfNull(addr);

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
			SDR::Error::ScopedContext e1("RelativeJumpFunctionFinder"s);
			
			SDR::Error::ThrowIfNull(address);

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

			SDR::Error::ThrowIfNull(Address);
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
