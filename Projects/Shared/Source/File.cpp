#include <SDR Shared\File.hpp>
#include <Shlwapi.h>

namespace SDR::File
{
	ScopedFile::ScopedFile(const char* path, const char* mode)
	{
		Assign(path, mode);
	}

	ScopedFile::ScopedFile(const std::string& path, const char* mode)
	{
		Assign(path.c_str(), mode);
	}

	ScopedFile::ScopedFile(const wchar_t* path, const wchar_t* mode)
	{
		Assign(path, mode);
	}

	ScopedFile::ScopedFile(const std::wstring& path, const wchar_t* mode)
	{
		Assign(path.c_str(), mode);
	}

	ScopedFile::~ScopedFile()
	{
		Close();
	}

	void ScopedFile::Close()
	{
		if (Handle)
		{
			fclose(Handle);
			Handle = nullptr;
		}
	}

	FILE* ScopedFile::Get() const
	{
		return Handle;
	}

	ScopedFile::operator bool() const
	{
		return Get() != nullptr;
	}

	void ScopedFile::Assign(const char* path, const char* mode)
	{
		Close();

		Handle = fopen(path, mode);

		if (!Handle)
		{
			throw ExceptionType::CouldNotOpenFile;
		}
	}

	void ScopedFile::Assign(const std::string& path, const char* mode)
	{
		Assign(path.c_str(), mode);
	}

	void ScopedFile::Assign(const wchar_t* path, const wchar_t* mode)
	{
		Close();

		Handle = _wfopen(path, mode);

		if (!Handle)
		{
			throw ExceptionType::CouldNotOpenFile;
		}
	}

	void ScopedFile::Assign(const std::wstring& path, const wchar_t* mode)
	{
		Assign(path.c_str(), mode);
	}

	int ScopedFile::GetStreamPosition() const
	{
		return ftell(Get());
	}

	void ScopedFile::SeekAbsolute(size_t pos)
	{
		fseek(Get(), pos, SEEK_SET);
	}

	void ScopedFile::SeekEnd()
	{
		fseek(Get(), 0, SEEK_END);
	}

	std::vector<uint8_t> ScopedFile::ReadAll()
	{
		SeekAbsolute(0);
		SeekEnd();

		auto size = GetStreamPosition();

		SeekAbsolute(0);

		std::vector<uint8_t> ret(size);

		fread_s(&ret[0], ret.size(), size, 1, Get());

		return ret;
	}

	std::string ScopedFile::ReadString()
	{
		SeekAbsolute(0);
		SeekEnd();

		auto size = GetStreamPosition();

		SeekAbsolute(0);

		std::string ret(size, 0);

		fread_s(&ret[0], ret.size(), size, 1, Get());

		return ret;
	}
}
