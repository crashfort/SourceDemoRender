#include "Profile.hpp"
#include "SDR Shared\Log.hpp"
#include <vector>

namespace
{
	std::vector<SDR::Profile::Entry> Entries;
}

int SDR::Profile::RegisterProfiling(const char* name)
{
	auto index = Entries.size();

	SDR::Profile::Entry entry;
	entry.Name = name;

	Entries.emplace_back(entry);

	return index;
}

std::chrono::nanoseconds SDR::Profile::GetTimeDifference(TimePointType start)
{
	using namespace std::chrono;

	auto now = GetTimeNow();
	auto difference = now - start;

	auto time = duration_cast<nanoseconds>(difference);

	return time;
}

SDR::Profile::ScopedEntry::ScopedEntry(int index) : Target(Entries[index]), Start(GetTimeNow())
{
	++Target.Calls;
}

SDR::Profile::ScopedEntry::~ScopedEntry()
{
	Target.TotalTime += GetTimeDifference(Start);
}

void SDR::Profile::Reset()
{
	for (auto& entry : Entries)
	{
		entry.Calls = 0;
		entry.TotalTime = 0ns;
	}
}

void SDR::Profile::ShowResults()
{
	int index = 0;

	for (const auto& entry : Entries)
	{
		if (entry.Calls > 0)
		{
			auto avg = entry.TotalTime / entry.Calls;
			auto ms = avg / 1.0ms;

			Log::Message("SDR: %s (%u): avg %0.4f ms\n", entry.Name, entry.Calls, ms);
		}

		++index;
	}
}
