#pragma once

namespace SDR
{
	struct ModuleInformation
	{
		ModuleInformation(const char* name) : Name(name)
		{
			MODULEINFO info;
			
			K32GetModuleInformation
			(
				GetCurrentProcess(),
				GetModuleHandleA(name),
				&info,
				sizeof(info)
			);

			MemoryBase = info.lpBaseOfDll;
			MemorySize = info.SizeOfImage;
		}

		const char* Name;

		void* MemoryBase;
		size_t MemorySize;
	};

	struct HookModuleBase
	{
		HookModuleBase
		(
			const char* module,
			const char* name,
			void* newfunc
		) :
			DisplayName(name),
			Module(module),
			NewFunction(newfunc)
		{

		}

		const char* DisplayName;
		const char* Module;

		void* TargetFunction;
		void* NewFunction;
		void* OriginalFunction;

		virtual MH_STATUS Create() = 0;
	};

	void Setup();
	void Close();

	constexpr auto MemoryPattern(const char* input)
	{
		return reinterpret_cast<const uint8_t*>(input);
	}

	void AddModule(HookModuleBase* module);

	using ShutdownFuncType = void(*)();
	void AddPluginShutdownFunction(ShutdownFuncType function);

	void* GetAddressFromPattern
	(
		const ModuleInformation& library,
		const uint8_t* pattern,
		const char* mask
	);

	struct PluginShutdownFunctionAdder
	{
		PluginShutdownFunctionAdder(ShutdownFuncType function)
		{
			AddPluginShutdownFunction(function);
		}
	};

	template <typename FuncSignature, bool Runtime = false>
	class HookModuleMask final : public HookModuleBase
	{
	public:
		HookModuleMask
		(
			const char* module,
			const char* name,
			FuncSignature newfunction,
			const uint8_t* pattern,
			const char* mask
		) :
			HookModuleBase(module, name, newfunction),
			Pattern(pattern),
			Mask(mask)
		{
			if (!Runtime)
			{
				AddModule(this);
			}
		}

		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		virtual MH_STATUS Create() override
		{
			ModuleInformation info(Module);
			TargetFunction = GetAddressFromPattern(info, Pattern, Mask);

			auto res = MH_CreateHookEx
			(
				TargetFunction,
				NewFunction,
				&OriginalFunction
			);

			return res;
		}

	private:
		const uint8_t* Pattern;
		const char* Mask;
	};

	template <typename FuncSignature, bool Runtime = false>
	class HookModuleStaticAddress final : public HookModuleBase
	{
	public:
		HookModuleStaticAddress
		(
			const char* module,
			const char* name,
			FuncSignature newfunction,
			uintptr_t address
		) :
			HookModuleBase(module, name, newfunction),
			Address(address)
		{
			if (!Runtime)
			{
				AddModule(this);
			}
		}

		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		virtual MH_STATUS Create() override
		{
			/*
				A module that is configured at runtime
				is assumed to have found the right address
			*/
			if (!Runtime)
			{
				/*
					IDA starts addresses at this value
				*/
				Address -= 0x10000000;
			
				ModuleInformation info(Module);
				Address += (uintptr_t)info.MemoryBase;
			}

			TargetFunction = (void*)Address;

			auto res = MH_CreateHookEx
			(
				TargetFunction,
				NewFunction,
				&OriginalFunction
			);

			return res;
		}

	private:
		uintptr_t Address;
	};

	template <typename FuncSignature, bool Runtime = false>
	class HookModuleAPI final : public HookModuleBase
	{
	public:
		HookModuleAPI
		(
			const char* module,
			const char* name,
			const char* exportname,
			FuncSignature newfunction
		) :
			HookModuleBase(module, name, newfunction),
			ExportName(exportname)
		{
			if (!Runtime)
			{
				AddModule(this);
			}
		}

		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		virtual MH_STATUS Create() override
		{
			wchar_t module[64];
			swprintf_s(module, L"%S", Module);

			auto res = MH_CreateHookApiEx
			(
				module,
				ExportName,
				NewFunction,
				&OriginalFunction,
				&TargetFunction
			);

			return res;
		}

	private:
		const char* ExportName;
	};

	struct AddressFinder
	{
		AddressFinder
		(
			const char* module,
			const uint8_t* pattern,
			const char* mask,
			int offset = 0
		)
		{
			auto addr = GetAddressFromPattern
			(
				module,
				pattern,
				mask
			);

			auto addrmod = static_cast<uint8_t*>(addr);

			/*
				Increment for any extra instruction
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
}
