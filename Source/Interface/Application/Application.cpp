#include "PrecompiledHeader.hpp"
#include "Application.hpp"
#include "dbg.h"

namespace
{
	struct Application
	{
		std::vector<SDR::HookModuleBase*> Modules;
		std::vector<SDR::ShutdownFuncType> OnCloseFunctions;
	};

	Application MainApplication;

	/*
		From Yalter's SPT
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

void SDR::Setup()
{
	auto res = MH_Initialize();

	if (res != MH_OK)
	{
		Warning("SDR: Failed to initialize hooks\n");
		throw res;
	}

	Msg("SDR: Creating %d modules\n", MainApplication.Modules.size());

	for (auto module : MainApplication.Modules)
	{
		auto res = module->Create();
		auto name = module->DisplayName;

		if (res == MH_OK)
		{
			auto library = module->Module;
			auto function = module->TargetFunction;

			MH_EnableHook(function);

			Msg("SDR: Enabled module \"%s\" -> %s @ %p\n", name, library, function);
		}

		else
		{
			Warning("SDR: Could not enable module \"%s\" - \"%s\"\n", name, MH_StatusToString(res));
			throw res;
		}
	}
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
	MainApplication.Modules.emplace_back(module);
}

void SDR::AddPluginShutdownFunction(ShutdownFuncType function)
{
	MainApplication.OnCloseFunctions.emplace_back(function);
}

void* SDR::GetAddressFromPattern(const ModuleInformation& library, const byte* pattern, const char* mask)
{
	return Memory::FindPattern(library.MemoryBase, library.MemorySize, pattern, mask);
}
