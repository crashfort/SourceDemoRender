#pragma once
#include <cstdio>
#include <cstdint>
#include <vector>

namespace SDR::File
{
	struct ScopedFile
	{
		enum class ExceptionType
		{
			CouldNotOpenFile
		};

		ScopedFile() = default;
		ScopedFile(const char* path, const char* mode);
		ScopedFile(const wchar_t* path, const wchar_t* mode);

		~ScopedFile();

		void Close();
		FILE* Get() const;
		explicit operator bool() const;

		void Assign(const char* path, const char* mode);
		void Assign(const wchar_t* path, const wchar_t* mode);

		int GetStreamPosition() const;
		void SeekAbsolute(size_t pos);
		void SeekEnd();

		template <typename... Types>
		bool WriteSimple(Types&&... args)
		{
			size_t adder[] =
			{
				[&]()
				{
					return fwrite(&args, sizeof(args), 1, Get());
				}()...
			};

			for (auto value : adder)
			{
				if (value == 0)
				{
					return false;
				}
			}

			return true;
		}

		size_t WriteRegion(const void* start, size_t size, int count = 1)
		{
			return fwrite(start, size, count, Get());
		}

		template <typename... Args>
		int WriteText(const char* format, Args&&... args)
		{
			return fprintf_s(Get(), format, std::forward<Args>(args)...);
		}

		template <typename... Types>
		bool ReadSimple(Types&... args)
		{
			size_t adder[] =
			{
				[&]()
				{
					return fread_s(&args, sizeof(args), sizeof(args), 1, Get());
				}()...
			};

			for (auto value : adder)
			{
				if (value == 0)
				{
					return false;
				}
			}

			return true;
		}

		template <typename T>
		size_t ReadRegion(std::vector<T>& vec, int count = 1)
		{
			return fread_s(&vec[0], vec.size() * sizeof(T), sizeof(T), count, Get());
		}

		std::vector<uint8_t> ReadAll();

		FILE* Handle = nullptr;
	};
}
