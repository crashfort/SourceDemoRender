#include "SDR Shared\String.hpp"
#include <codecvt>

namespace
{
	auto& GetConverter()
	{
		static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter;
	}
}

std::string SDR::String::ToUTF8(const std::wstring& input)
{
	return GetConverter().to_bytes(input);
}

std::string SDR::String::ToUTF8(const wchar_t* input)
{
	return GetConverter().to_bytes(input);
}

std::wstring SDR::String::FromUTF8(const std::string& input)
{
	return GetConverter().from_bytes(input);
}

std::wstring SDR::String::FromUTF8(const char* input)
{
	return GetConverter().from_bytes(input);
}
