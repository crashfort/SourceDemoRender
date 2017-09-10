#include "Pool.hpp"
#include "Profile.hpp"
#include <thread>

namespace
{
	namespace LocalProfiling
	{
		int PoolInsert;
		int PoolWait;

		SDR::PluginStartupFunctionAdder A1("Pool profiling", []()
		{
			PoolInsert = SDR::Profile::RegisterProfiling("PoolInsert");
			PoolWait = SDR::Profile::RegisterProfiling("PoolWait");
		});
	}
}

size_t SDR::Pool::FramePool::GetMaxSize()
{
	return INT32_MAX / 2;
}

void SDR::Pool::FramePool::Create(size_t itemsize, int parts)
{
	auto maxsize = GetMaxSize();
	auto maxitems = maxsize / itemsize;

	Buffer.resize(maxitems * itemsize);
	LockedEntries.resize(maxitems);

	ItemParts = parts;
	RemainingInsertParts = ItemParts;
}

bool SDR::Pool::FramePool::Lock(HandleData& handle)
{
	if (handle.LockedPtr->Locked)
	{
		return false;
	}

	++LockCount;
	handle.LockedPtr->Locked = true;
	return true;
}

void SDR::Pool::FramePool::Unlock(HandleData& handle)
{
	--LockCount;
	handle.LockedPtr->Locked = false;
}

SDR::Pool::HandleData SDR::Pool::FramePool::Insert(const void* source, size_t size)
{
	Profile::ScopedEntry e1(LocalProfiling::PoolInsert);

	HandleData ret;

	if (LockIndex == LockedEntries.size())
	{
		UsedSize = 0;
		LockIndex = 0;
	}

	ret.LockedPtr = &LockedEntries[LockIndex];

	if (RemainingInsertParts == ItemParts)
	{
		if (!Lock(ret))
		{
			return nullptr;
		}
	}

	auto ptr = Buffer.data();
	ptr += UsedSize;

	std::memcpy(ptr, source, size);

	ret.Ptr = ptr;
	ret.Size = size;

	UsedSize += size;
	--RemainingInsertParts;

	if (RemainingInsertParts == 0)
	{
		RemainingGetParts = ItemParts;
		RemainingInsertParts = ItemParts;
		++LockIndex;
	}

	return ret;
}

std::array<SDR::Pool::HandleData, 3> SDR::Pool::FramePool::Insert(std::array<std::pair<const void*, size_t>, 3>&& list)
{
	int index = 0;
	std::array<HandleData, 3> ret;

	for (auto entry : list)
	{
		ret[index] = Insert(entry.first, entry.second);
		++index;
	}

	return ret;
}

void* SDR::Pool::FramePool::Get(HandleData& handle)
{
	if (!handle.LockedPtr->Locked)
	{
		return nullptr;
	}

	if (RemainingGetParts == 0)
	{
		return nullptr;
	}

	--RemainingGetParts;
	return handle.Ptr;
}

void SDR::Pool::FramePool::WaitIfFull() const
{
	if (LockCount == LockedEntries.size())
	{
		Profile::ScopedEntry e1(LocalProfiling::PoolWait);

		while (LockCount)
		{
			std::this_thread::sleep_for(1ms);
		}
	}
}
