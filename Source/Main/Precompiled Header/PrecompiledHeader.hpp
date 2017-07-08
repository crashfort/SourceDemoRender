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
#include <comdef.h>

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

namespace SDR::Error
{
	struct Exception
	{
		char Description[256];
	};

	template <typename... Args>
	inline Exception Make(const char* format, Args&&... args)
	{
		Exception info;
		sprintf_s(info.Description, format, std::forward<Args>(args)...);

		throw info;
	}

	template <typename T, typename... Args>
	inline void ThrowIfNull(const T* ptr, const char* format, Args&&... args)
	{
		if (!ptr)
		{
			Make(format, std::forward<Args>(args)...);
		}
	}

	namespace LAV
	{
		template <typename T, typename... Args>
		inline void ThrowIfFailed(T code, const char* format, Args&&... args)
		{
			if (code < 0)
			{
				Make(format, std::forward<Args>(args)...);
			}
		}
	}

	namespace MS
	{
		template <typename... Args>
		inline void ThrowIfFailed(HRESULT hr, const char* format, Args&&...args)
		{
			if (FAILED(hr))
			{
				_com_error error(hr);
				auto message = error.ErrorMessage();

				char start[sizeof(Exception::Description)];
				char end[sizeof(Exception::Description)];

				sprintf_s(end, format, std::forward<Args>(args)...);
				sprintf_s(start, "%08X (%s) -> ", hr, message);
				strcat_s(end, start);

				Make(end);
			}
		}
	}
}

