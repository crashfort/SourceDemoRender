#include <SDR Shared\String.hpp>
#include <codecvt>

namespace
{
	using ConvertType = std::wstring_convert<std::codecvt_utf8<wchar_t>>;
}

std::string SDR::String::ToUTF8(const std::wstring& input)
{
	ConvertType converter;
	return converter.to_bytes(input);
}

std::string SDR::String::ToUTF8(const wchar_t* input)
{
	ConvertType converter;
	return converter.to_bytes(input);
}

std::wstring SDR::String::FromUTF8(const std::string& input)
{
	ConvertType converter;
	return converter.from_bytes(input);
}

std::wstring SDR::String::FromUTF8(const char* input)
{
	ConvertType converter;
	return converter.from_bytes(input);
}
