#include <SDR Shared\String.hpp>
#include <SDR Shared\BareWindows.hpp>

std::string SDR::String::ToUTF8(const std::wstring& input)
{
	std::string ret;

	auto size = WideCharToMultiByte(CP_UTF8, 0, input.data(), input.size(), nullptr, 0, nullptr, nullptr);
	ret.resize(size, 0);

	WideCharToMultiByte(CP_UTF8, 0, input.data(), input.size(), &ret.front(), size, nullptr, nullptr);

	return ret;
}

std::string SDR::String::ToUTF8(const wchar_t* input)
{
	return ToUTF8(std::wstring(input));
}

std::wstring SDR::String::FromUTF8(const std::string& input)
{
	std::wstring ret;

	auto size = MultiByteToWideChar(CP_UTF8, 0, input.data(), input.size(), nullptr, 0);
	ret.resize(size, 0);

	MultiByteToWideChar(CP_UTF8, 0, input.data(), input.size(), &ret.front(), size);

	return ret;
}

std::wstring SDR::String::FromUTF8(const char* input)
{
	return FromUTF8(std::string(input));
}
