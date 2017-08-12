#pragma once
#include <string>

using namespace std::string_literals;

namespace SDR::String
{
	template <typename... Args>
	inline std::string GetFormattedString(const char* format, Args&&... args)
	{
		std::string ret;

		auto size = std::snprintf(nullptr, 0, format, std::forward<Args>(args)...);
		ret.resize(size + 1);

		std::snprintf(ret.data(), size + 1, format, std::forward<Args>(args)...);

		/*
			Remove null terminator that above function adds.
		*/
		ret.pop_back();

		return ret;
	}

	template <typename T>
	bool EndsWith(const T* str, const T* end)
	{
		auto sourcelength = std::char_traits<T>::length(str);
		auto endlength = std::char_traits<T>::length(end);

		if (sourcelength < endlength)
		{
			return false;
		}

		auto start = str + sourcelength - endlength;
		return std::char_traits<T>::compare(start, end, endlength) == 0;
	}
}
