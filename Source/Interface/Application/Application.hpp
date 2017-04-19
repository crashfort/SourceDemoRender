#pragma once

namespace SDR
{
	struct ModuleInformation
	{
		ModuleInformation(const char* name) : Name(name)
		{
			MODULEINFO info;
			K32GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(name), &info, sizeof(MODULEINFO));

			MemoryBase = info.lpBaseOfDll;
			MemorySize = info.SizeOfImage;
		}

		const char* Name;

		void* MemoryBase;
		size_t MemorySize;
	};

	struct HookModuleBase
	{
		HookModuleBase(const char* module, const char* name, void* newfunc) :
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

	template <typename FuncSignature>
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
			AddModule(this);
		}

		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		virtual MH_STATUS Create() override
		{
			ModuleInformation info(Module);
			TargetFunction = GetAddressFromPattern(info, Pattern, Mask);

			return MH_CreateHookEx(TargetFunction, NewFunction, &OriginalFunction);
		}

	private:
		const uint8_t* Pattern;
		const char* Mask;
	};

	template <typename FuncSignature>
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
			AddModule(this);
		}

		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		virtual MH_STATUS Create() override
		{
			/*
				IDA starts addresses at this value
			*/
			Address -= 0x10000000;
			
			ModuleInformation info(Module);
			Address += (uintptr_t)info.MemoryBase;

			TargetFunction = (void*)Address;

			return MH_CreateHookEx(TargetFunction, NewFunction, &OriginalFunction);
		}

	private:
		uintptr_t Address;
	};

	template <typename FuncSignature>
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
			AddModule(this);
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
}
