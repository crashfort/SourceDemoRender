#pragma once

#include "targetver.h"

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

#include "MinHookCPP.hpp"

using namespace std::chrono_literals;

#include "eiface.h"
#include "cdll_int.h"
#include "engine\iserverplugin.h"
#include "tier2\tier2.h"
#include "game\server\iplayerinfo.h"
#include "convar.h"
#include "utlbuffer.h"
#include "filesystem.h"

namespace SDR
{
	struct EngineInterfaces
	{
		IPlayerInfoManager* PlayerInfoManager;
		CGlobalVars* Globals;
		IVEngineClient* EngineClient;
		IFileSystem* FileSystem;
	};

	const EngineInterfaces& GetEngineInterfaces();
}
