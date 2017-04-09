#pragma once

#include "TargetVersion.hpp"

#include <Windows.h>
#include <Psapi.h>
#include <ShlObj.h>

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include <thread>
#include <atomic>

using namespace std::chrono_literals;

#include "MinHookCPP.hpp"

#include "engine\iserverplugin.h"
#include "cdll_int.h"
#include "tier1\tier1.h"
#include "tier2\tier2.h"
#include "convar.h"
#include "materialsystem\imaterialsystem.h"

namespace SDR
{
	struct EngineInterfaces
	{
		IVEngineClient* EngineClient;
	};

	const EngineInterfaces& GetEngineInterfaces();
}
