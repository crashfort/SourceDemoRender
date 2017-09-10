#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <atomic>

namespace SDR::Pool
{
	struct LockedData
	{
		bool Locked = false;
	};

	struct HandleData
	{
		HandleData() = default;

		HandleData(uint8_t* ptr) : Ptr(ptr)
		{

		}

		explicit operator bool() const
		{
			return Ptr != nullptr;
		}

		uint8_t* Ptr = nullptr;
		LockedData* LockedPtr = nullptr;
		size_t Size = 0;
	};

	struct FramePool
	{
		static size_t GetMaxSize();

		void Create(size_t itemsize, int parts);

		bool Lock(HandleData& handle);
		void Unlock(HandleData& handle);

		HandleData Insert(const void* source, size_t size);
		std::array<HandleData, 3> Insert(std::array<std::pair<const void*, size_t>, 3>&& list);

		void* Get(HandleData& handle);

		void WaitIfFull() const;

		std::vector<uint8_t> Buffer;
		std::vector<LockedData> LockedEntries;

		std::atomic_int LockCount = 0;
		size_t LockIndex = 0;
		int ItemParts = 0;
		size_t ItemSize = 0;
		std::atomic_int RemainingInsertParts = 0;
		std::atomic_int RemainingGetParts = 0;
		size_t UsedSize = 0;
	};
}
