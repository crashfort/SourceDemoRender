#pragma once
#include <SDR Shared\String.hpp>

namespace SDR::Library
{
	struct InitializeData
	{
		const char* ResourcePath;
		const char* GamePath;
		
		HWND LauncherCLI;
	};

	using SDR_LibraryVersion = int(__cdecl*)();
	using SDR_Initialize = void(__cdecl*)(const InitializeData& data);
}
