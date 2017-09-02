#pragma once
#include <chrono>
#include <array>

using namespace std::chrono_literals;

namespace SDR::Profile
{
	namespace Types
	{
		enum Type
		{
			PushYUV,
			PushRGB,
			Encode,

			Count
		};
	}

	inline auto GetTimeNow()
	{
		return std::chrono::high_resolution_clock::now();
	}

	using TimePointType = decltype(GetTimeNow());

	std::chrono::nanoseconds GetTimeDifference(TimePointType start);

	struct Entry
	{
		uint32_t Calls = 0;
		std::chrono::nanoseconds TotalTime = 0ns;
	};

	struct ScopedEntry
	{
		ScopedEntry(Types::Type entry);
		~ScopedEntry();

		TimePointType Start;
		Entry& Target;
	};

	void Reset();
	void ShowResults();
}
