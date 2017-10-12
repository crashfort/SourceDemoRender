#pragma once
#include <chrono>
#include <array>
#include "Interface\Application\Application.hpp"

using namespace std::chrono_literals;

namespace SDR::Profile
{
	int RegisterProfiling(const char* name);

	inline auto GetTimeNow()
	{
		return std::chrono::high_resolution_clock::now();
	}

	using TimePointType = decltype(GetTimeNow());

	std::chrono::nanoseconds GetTimeDifference(TimePointType start);

	struct Entry
	{
		const char* Name;
		uint32_t Calls = 0;
		std::chrono::nanoseconds TotalTime = 0ns;
	};

	struct ScopedEntry
	{
		ScopedEntry(int index);
		~ScopedEntry();

		TimePointType Start;
		Entry& Target;
	};

	void Reset();
	void ShowResults();
}
