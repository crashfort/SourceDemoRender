#pragma once

#include "targetver.h"

#include <Windows.h>
#include <Psapi.h>

#include <ShlObj.h>

#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <thread>
#include <future>
#include <memory>
#include <climits>

#include "MinHookCPP.hpp"

using namespace std::chrono_literals;

#include "cdll_int.h"
#include "engine\iserverplugin.h"
#include "tier2\tier2.h"
#include "game\server\iplayerinfo.h"
#include "convar.h"
#include "utlbuffer.h"

namespace SDR
{
	struct EngineInterfaces
	{
		IPlayerInfoManager* PlayerInfoManager;
		CGlobalVars* Globals;
	};

	const EngineInterfaces& GetEngineInterfaces();
}
