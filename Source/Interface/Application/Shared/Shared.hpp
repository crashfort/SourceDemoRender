#pragma once

namespace SDR::Shared
{
	inline ConVar MakeBool(const char* name, const char* value)
	{
		return ConVar(name, value, FCVAR_NEVER_AS_STRING, "", true, 0, true, 1);
	}

	template <typename T>
	inline ConVar MakeNumber(const char* name, const char* value, T min, T max)
	{
		return ConVar(name, value, FCVAR_NEVER_AS_STRING, "", true, min, true, max);
	}

	template <typename T>
	inline ConVar MakeNumberWithString(const char* name, const char* value, T min, T max)
	{
		return ConVar(name, value, 0, "", true, min, true, max);
	}

	template <typename T>
	inline ConVar MakeNumber(const char* name, const char* value, T min)
	{
		return ConVar(name, value, FCVAR_NEVER_AS_STRING, "", true, min, false, 0);
	}

	inline ConVar MakeString(const char* name, const char* value)
	{
		return ConVar(name, value, 0, "");
	}

	struct ScopedFile
	{
		enum class ExceptionType
		{
			CouldNotOpenFile
		};

		ScopedFile() = default;

		ScopedFile(const char* path, const char* mode)
		{
			Assign(path, mode);
		}

		~ScopedFile()
		{
			Close();
		}

		void Close()
		{
			if (Handle)
			{
				fclose(Handle);
				Handle = nullptr;
			}
		}

		auto Get() const
		{
			return Handle;
		}

		explicit operator bool() const
		{
			return Get() != nullptr;
		}

		void Assign(const char* path, const char* mode)
		{
			Close();

			Handle = fopen(path, mode);

			if (!Handle)
			{
				throw ExceptionType::CouldNotOpenFile;
			}
		}

		int GetStreamPosition() const
		{
			return ftell(Get());
		}

		void SeekAbsolute(size_t pos)
		{
			fseek(Get(), pos, SEEK_SET);
		}

		void SeekEnd()
		{
			fseek(Get(), 0, SEEK_END);
		}

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
		int WriteText(const char* format,Args&&... args)
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

		std::vector<uint8_t> ReadAll()
		{
			SeekAbsolute(0);
			SeekEnd();

			auto size = GetStreamPosition();

			SeekAbsolute(0);

			std::vector<uint8_t> ret(size);

			fread_s(&ret[0], ret.size(), size, 1, Get());

			return ret;
		}

		FILE* Handle = nullptr;
	};
}
