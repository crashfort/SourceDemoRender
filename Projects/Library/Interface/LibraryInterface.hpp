#pragma once
#include <functional>

namespace SDR::Library
{
	void Load();
	void Unload();

	const char* GetGamePath();
	const char* GetGameName();

	bool IsGame(const char* test);

	std::string BuildPath(const char* file);
}
