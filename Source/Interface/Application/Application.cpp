#include "PrecompiledHeader.hpp"
#include "Application.hpp"
#include "dbg.h"

namespace
{
	struct Application
	{
		std::vector<SDR::HookModuleBase*> Modules;
		std::vector<void(*)()> OnCloseFunctions;
	};

	Application MainApplication;

	/*
		From Yalter SPT
	*/
	namespace Memory
	{
		inline bool DataCompare(const byte* data, const byte* pattern, const char* mask)
		{
			for (; *mask != 0; ++data, ++pattern, ++mask)
			{
				if (*mask == 'x' && *data != *pattern)
				{
					return false;
				}
			}

			return (*mask == 0);
		}

		void* FindPattern(const void* start, size_t length, const byte* pattern, const char* mask)
		{
			auto masklength = strlen(mask);
			
			for (size_t i = 0; i <= length - masklength; ++i)
			{
				auto addr = reinterpret_cast<const byte*>(start) + i;
				
				if (DataCompare(addr, pattern, mask))
				{
					return const_cast<void*>(reinterpret_cast<const void*>(addr));
				}
			}

			return nullptr;
		}
	}
}

bool SDR::Setup()
{
	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		Warning("SDR: Failed to initialize hooks\n");
		return false;
	}

	Msg("SDR: Creating %d modules\n", MainApplication.Modules.size());

	for (auto module : MainApplication.Modules)
	{
		auto res = module->Create();
		auto name = module->GetName();

		if (res == MH_OK)
		{
			auto library = module->GetLibrary();
			auto function = module->GetTargetFunction();

			MH_EnableHook(function);

			Msg("SDR: Enabled module \"%s\" -> %s @ 0x%08x\n", name, library, function);
		}

		else
		{
			Warning("SDR: Could not enable module \"%s\" - \"%s\"\n", name, MH_StatusToString(res));
		}
	}

	return true;
}

void SDR::Close()
{
	for (auto&& func : MainApplication.OnCloseFunctions)
	{
		func();
	}

	MH_Uninitialize();
}

void SDR::AddModule(HookModuleBase* module)
{
	MainApplication.Modules.push_back(module);
}

void SDR::AddPluginShutdownFunction(ShutdownFuncType function)
{
	MainApplication.OnCloseFunctions.push_back(function);
}

void* SDR::GetAddressFromPattern(const LibraryModuleBase& library, const byte* pattern, const char* mask)
{
	return Memory::FindPattern(library.MemoryBase, library.MemorySize, pattern, mask);
}
