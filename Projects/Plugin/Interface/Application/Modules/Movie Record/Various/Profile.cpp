#include "Profile.hpp"
#include "SDR Shared\Log.hpp"

namespace
{
	const char* Names[] =
	{
		"PushYUV",
		"PushRGB",
		"Encode",
	};

	std::array<SDR::Profile::Entry, SDR::Profile::Types::Count> Entries;
}

std::chrono::nanoseconds SDR::Profile::GetTimeDifference(TimePointType start)
{
	using namespace std::chrono;

	auto now = GetTimeNow();
	auto difference = now - start;

	auto time = duration_cast<nanoseconds>(difference);

	return time;
}

SDR::Profile::ScopedEntry::ScopedEntry(Types::Type entry) : Target(Entries[entry]), Start(GetTimeNow())
{
	++Target.Calls;
}

SDR::Profile::ScopedEntry::~ScopedEntry()
{
	Target.TotalTime += GetTimeDifference(Start);
}

void SDR::Profile::Reset()
{
	Entries.fill({});
}

void SDR::Profile::ShowResults()
{
	int index = 0;

	for (const auto& entry : Entries)
	{
		if (entry.Calls > 0)
		{
			auto name = Names[index];
			auto avg = entry.TotalTime / entry.Calls;
			auto ms = avg / 1.0ms;

			Log::Message("SDR: %s (%u): avg %0.4f ms\n", name, entry.Calls, ms);
		}

		++index;
	}
}
