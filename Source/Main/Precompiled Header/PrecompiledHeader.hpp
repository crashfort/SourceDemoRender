#pragma once

#include "TargetVersion.hpp"

#include <Windows.h>

#include <Psapi.h>
#include <ShlObj.h>

#include <wrl.h>

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include <thread>
#include <atomic>
#include <cctype>
#include <array>

#include "rapidjson\document.h"

using namespace std::chrono_literals;

#include "MinHookCPP.hpp"

#include "engine\iserverplugin.h"
#include "cdll_int.h"
#include "tier1\tier1.h"
#include "tier2\tier2.h"
#include "convar.h"
#include "materialsystem\imaterialsystem.h"

#include "Interface\Application\Shared\Shared.hpp"

#undef Verify

namespace SDR
{
	struct EngineInterfaces
	{
		IVEngineClient* EngineClient;
	};

	const char* GetGamePath();
	const char* GetGameName();

	const EngineInterfaces& GetEngineInterfaces();
}

namespace MS
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			Warning("SDR: ThrowIfFailed: %08X\n", hr);
			throw hr;
		}
	}
}
