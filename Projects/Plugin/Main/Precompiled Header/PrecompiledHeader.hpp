#pragma once
#include "SDR Shared\BareWindows.hpp"
#include <Psapi.h>
#include <Shlwapi.h>
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

#include "Interface\Application\Modules\Shared\Shared.hpp"
#include "SDR Shared\File.hpp"

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
		std::string Description;
	};

	inline void Print(const Exception& error)
	{
		Warning("SDR: %s\n", error.Description.c_str());
	}

	template <typename... Args>
	inline std::string GetFormattedString(const char* format, Args&&... args)
	{
		std::string ret;

		auto size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...);
		ret.resize(size + 1);

		std::snprintf(ret.data(), size + 1, format, std::forward<Args>(args)...);

		/*
			Remove null terminator that above function adds.
		*/
		ret.pop_back();

		return ret;
	}

	/*
		For use with unrecoverable errors.
	*/
	inline void Make(std::string&& str)
	{
		Exception info;
		info.Description = std::move(str);

		Print(info);
		throw info;
	}

	/*
		For use with unrecoverable errors.
	*/
	template <typename... Args>
	inline void Make(const char* format, Args&&... args)
	{
		Make(GetFormattedString(format, std::forward<Args>(args)...));
		
	}

	/*
		For use with unrecoverable errors.
	*/
	template <typename... Args>
	inline void ThrowIfNull(const void* ptr, const char* format, Args&&... args)
	{
		if (ptr == nullptr)
		{
			Make(format, std::forward<Args>(args)...);
		}
	}

	namespace LAV
	{
		/*
			For use with unrecoverable errors.
		*/
		template <typename... Args>
		inline void ThrowIfFailed(int code, const char* format, Args&&... args)
		{
			if (code < 0)
			{
				Make(format, std::forward<Args>(args)...);
			}
		}
	}

	namespace MS
	{
		/*
			For use with unrecoverable errors.
		*/
		template <typename... Args>
		inline void ThrowIfFailed(HRESULT hr, const char* format, Args&&... args)
		{
			if (FAILED(hr))
			{
				_com_error error(hr);
				auto message = error.ErrorMessage();

				auto user = GetFormattedString(format, std::forward<Args>(args)...);
				auto final = GetFormattedString("%08X (%s) -> ", hr, message);

				final += user;

				Make(std::move(final));
			}
		}
	}
}
