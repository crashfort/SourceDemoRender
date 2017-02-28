#pragma once

namespace SDR
{
	bool Setup();
	void Close();

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

	void AddModule(HookModuleBase* module);

	using ShutdownFuncType = void(*)();
	void AddPluginShutdownFunction(ShutdownFuncType function);

	class PluginShutdownFunctionAdder final
	{
	public:
		PluginShutdownFunctionAdder(ShutdownFuncType function)
		{
			AddPluginShutdownFunction(function);
		}
	};

	void* GetAddressFromPattern
	(
		const ModuleInformation& library,
		const byte* pattern,
		const char* mask
	);

	template <typename FuncSignature>
	class HookModuleMask final : public HookModuleBase
	{
	public:
		HookModuleMask
		(
			const char* module,
			const char* name,
			FuncSignature newfunction,
			const byte* pattern,
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
		const byte* Pattern;
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
}

/*
	Ugly but the patterns are unsigned chars anyway
*/
#define SDR_PATTERN(pattern) reinterpret_cast<const byte*>(pattern)
