#pragma once
#include <functional>

namespace SDR::Library
{
	void Load();

	const char* GetResourcePath();
	const char* GetGamePath();

	std::string BuildResourcePath(const char* file);
}
