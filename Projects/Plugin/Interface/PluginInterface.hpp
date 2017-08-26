#pragma once
#include <functional>

namespace SDR::Plugin
{
	void Load();
	void Unload();

	void SetAcceptFunction(std::function<void()>&& func);
}
