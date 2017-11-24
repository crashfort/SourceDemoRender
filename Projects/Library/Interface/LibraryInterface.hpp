#pragma once
#include <functional>

namespace SDR::Library
{
	void Load();
	void Unload();

	const char* GetResourcePath();
	const char* GetGameName();

	bool IsGame(const char* test);

	std::string BuildResourcePath(const char* file);
}
