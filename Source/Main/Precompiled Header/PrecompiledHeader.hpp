#pragma once

#define _WIN32_WINNT 0x0601
#include "TargetVersion.hpp"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define NOATOM
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOBITMAP
#define NOMINMAX
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSOUND
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOCRYPT
#define NOMCX

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

#include "valve_minmax_off.h"

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
