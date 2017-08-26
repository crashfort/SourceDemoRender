#pragma once
#include <wrl.h>
#include <initializer_list>
#include "Error.hpp"

namespace SDR::IPC
{
	HANDLE WaitForOne(std::initializer_list<HANDLE> handles);
}
