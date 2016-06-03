#pragma once

namespace SDR
{
	bool Setup();
	void Close();

	struct LibraryModuleBase
	{
		LibraryModuleBase(const char* name) :
			Name(name)
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

	class HookModuleBase
	{
	public:
		HookModuleBase(const char* library, const char* name, void* newfunction) :
			Library(library),
			Name(name),
			NewFunction(newfunction)
		{

		}

		virtual MH_STATUS Create() = 0;

		void* GetTargetFunction() const
		{
			return TargetFunction;
		}

		void* GetOriginalFunction() const
		{
			return OriginalFunction;
		}

		void* GetNewFunction() const
		{
			return NewFunction;
		}

		const char* GetLibrary() const
		{
			return Library;
		}

		const char* GetName() const
		{
			return Name;
		}

	protected:
		const char* Library;
		const char* Name;

		void* TargetFunction;
		void* OriginalFunction;
		void* NewFunction;
	};

	void AddModule(HookModuleBase* module);

	void* GetAddressFromPattern(const LibraryModuleBase& library, const byte* pattern, const char* mask);

	template <typename FuncSignature>
	class HookModuleMask final : public HookModuleBase
	{
	public:
		HookModuleMask(const char* library, const char* name, FuncSignature newfunction, const byte* pattern, const char* mask) :
			HookModuleBase(library, name, newfunction),
			Pattern(pattern),
			Mask(mask)
		{
			AddModule(this);
		}

		virtual MH_STATUS Create() override
		{
			LibraryModuleBase info(Library);
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
		HookModuleStaticAddress(const char* library, const char* name, FuncSignature newfunction, uintptr_t address) :
			HookModuleBase(library, name, newfunction),
			Address(address)
		{
			AddModule(this);
		}

		virtual MH_STATUS Create() override
		{
			/*
				IDA starts addresses at this value
			*/
			Address -= 0x10000000;
			
			LibraryModuleBase library(Library);
			Address += (size_t)library.MemoryBase;

			TargetFunction = (void*)Address;

			return MH_CreateHookEx(TargetFunction, NewFunction, &OriginalFunction);
		}

	private:
		uintptr_t Address;
	};

	class PointerModuleStaticAddress final : public HookModuleBase
	{
	public:
		PointerModuleStaticAddress(const char* library, const char* name, uintptr_t address) :
			HookModuleBase(library, name, nullptr),
			Address(address)
		{
			AddModule(this);
		}

		virtual MH_STATUS Create() override
		{
			/*
				IDA starts addresses at this value
			*/
			Address -= 0x10000000;
			
			LibraryModuleBase library(Library);
			Address += (size_t)library.MemoryBase;

			TargetFunction = (void*)Address;

			auto res = ReadProcessMemory(GetCurrentProcess(), TargetFunction, &TargetFunction, sizeof(TargetFunction), nullptr);

			if (!res)
			{
				return MH_ERROR_FUNCTION_NOT_FOUND;
			}

			return MH_OK;
		}
		
	private:
		uintptr_t Address;
	};
}

/*
	Ugly but the patterns are unsigned chars anyway
*/
#define SDR_PATTERN(pattern) reinterpret_cast<const byte*>(pattern)
