#pragma once
#include "String.hpp"
#include "Color.hpp"

namespace SDR::Log
{
	using LogFunctionType = void(*)(std::string&& text);
	using LogFunctionColorType = void(*)(Shared::Color col, std::string&& text);

	void SetMessageFunction(LogFunctionType func);
	void SetMessageColorFunction(LogFunctionColorType func);
	void SetWarningFunction(LogFunctionType func);

	void Message(std::string&& text);
	void MessageColor(Shared::Color col, std::string&& text);
	void Warning(std::string&& text);

	template <typename... Args>
	void Message(const char* format, Args&&... args)
	{
		Message(String::Format(format, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void MessageColor(Shared::Color col, const char* format, Args&&... args)
	{
		MessageColor(col, String::Format(format, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void Warning(const char* format, Args&&... args)
	{
		Warning(String::Format(format, std::forward<Args>(args)...));
	}
}
