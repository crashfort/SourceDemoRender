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
#include <functional>

#include "rapidjson\document.h"

using namespace std::chrono_literals;

#include "SDR Shared\MinHookCPP.hpp"

#include "Interface\Application\Modules\Shared\Console.hpp"
#include "SDR Shared\File.hpp"
#include "SDR Shared\Error.hpp"