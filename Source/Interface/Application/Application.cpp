#include "PrecompiledHeader.hpp"
#include "Application.hpp"
#include "dbg.h"

namespace
{
	struct Application
	{
		std::vector<SDR::HookModuleBase*> Modules;
		std::vector<SDR::StartupFuncData> StartupFunctions;
		std::vector<SDR::ShutdownFuncType> ShutdownFunctions;
	};

	Application MainApplication;

	/*
		From Yalter's SPT
	*/
	namespace Memory
	{
		inline bool DataCompare
		(
			const uint8_t* data,
			const uint8_t* pattern,
			const char* mask
		)
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

		void* FindPattern
		(
			const void* start,
			size_t length,
			const uint8_t* pattern,
			const char* mask
		)
		{
			auto masklength = strlen(mask);
			
			for (size_t i = 0; i <= length - masklength; ++i)
			{
				auto addr = reinterpret_cast<const uint8_t*>(start) + i;
				
				if (DataCompare(addr, pattern, mask))
				{
					return const_cast<void*>
					(
						reinterpret_cast<const void*>(addr)
					);
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
		Warning
		(
			"SDR: Failed to initialize hooks\n"
		);

		throw res;
	}

	Msg
	(
		"SDR: Creating %d modules\n",
		MainApplication.Modules.size()
	);

	for (auto module : MainApplication.Modules)
	{
		auto res = module->Create();
		auto name = module->DisplayName;

		if (res != MH_OK)
		{
			Warning
			(
				"SDR: Could not enable module \"%s\": \"%s\"\n",
				name,
				MH_StatusToString(res)
			);

			throw res;
		}

		auto library = module->Module;
		auto function = module->TargetFunction;

		MH_EnableHook(function);

		Msg
		(
			"SDR: Enabled module \"%s\" -> %s @ 0x%p\n",
			name,
			library,
			function
		);
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
	for (auto entry : MainApplication.StartupFunctions)
	{
		auto res = entry.Function();

		if (!res)
		{
			throw entry.Name;
		}
	}
}

void SDR::AddPluginShutdownFunction
(
	ShutdownFuncType function
)
{
	MainApplication.ShutdownFunctions.emplace_back(function);
}

void SDR::AddModule(HookModuleBase* module)
{
	MainApplication.Modules.emplace_back(module);
}

void* SDR::GetAddressFromPattern
(
	const ModuleInformation& library,
	const uint8_t* pattern,
	const char* mask
)
{
	return Memory::FindPattern
	(
		library.MemoryBase,
		library.MemorySize,
		pattern,
		mask
	);
}
