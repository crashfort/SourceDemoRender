#pragma once
#include <cstdint>

namespace SDR::API
{
	enum class InitializeCode : int32_t
	{
		GeneralFailure,
		Success,
		CouldNotInitializeHooks,
		CouldNotCreateLibraryIntercepts,
		CouldNotEnableLibraryIntercepts,
	};

	using SDR_LibraryVersion = int(__cdecl*)();
	using SDR_Initialize = InitializeCode(__cdecl*)(const char* path, const char* game);
	using SDR_Shutdown = void(__cdecl*)();
}
