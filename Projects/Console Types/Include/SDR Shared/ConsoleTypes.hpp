#pragma once

namespace SDR::Console::Types
{
	using CommandCallbackVoidType = void(*)();
	using CommandCallbackArgsType = void(*)(const void* ptr);
}
