#pragma once
#include "String.hpp"
#include "Color.hpp"

namespace SDR::Log
{
	using LogFunctionType = void(*)(const char* text);
	using LogFunctionColorType = void(*)(Shared::Color col, const char* text);

	void SetMessageFunction(LogFunctionType func);
	void SetMessageColorFunction(LogFunctionColorType func);
	void SetWarningFunction(LogFunctionType func);

	void Message(const char* text);
	void MessageColor(Shared::Color col, const char* text);
	void Warning(const char* text);

	template <typename... Args>
	void Message(const char* format, Args&&... args)
	{
		auto text = String::Format(format, std::forward<Args>(args)...);
		Message(text.c_str());
	}

	template <typename... Args>
	void MessageColor(Shared::Color col, const char* format, Args&&... args)
	{
		auto text = String::Format(format, std::forward<Args>(args)...);
		MessageColor(col, text.c_str());
	}

	template <typename... Args>
	void Warning(const char* format, Args&&... args)
	{
		auto text = String::Format(format, std::forward<Args>(args)...);
		Warning(text.c_str());
	}
}
