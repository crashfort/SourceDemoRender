#include "SDR Shared\Log.hpp"

namespace
{
	void MessageDefault(std::string&& text)
	{
		printf_s(text.c_str());
	}

	void MessageColorDefault(SDR::Shared::Color col, std::string&& text)
	{
		printf_s(text.c_str());
	}
	
	void WarningDefault(std::string&& text)
	{
		printf_s(text.c_str());
	}

	SDR::Log::LogFunctionType MessageImpl = MessageDefault;
	SDR::Log::LogFunctionColorType MessageColorImpl = MessageColorDefault;
	SDR::Log::LogFunctionType WarningImpl = WarningDefault;
}

void SDR::Log::SetMessageFunction(LogFunctionType func)
{
	MessageImpl = func;
}

void SDR::Log::SetMessageColorFunction(LogFunctionColorType func)
{
	MessageColorImpl = func;
}

void SDR::Log::SetWarningFunction(LogFunctionType func)
{
	WarningImpl = func;
}

void SDR::Log::Message(std::string&& text)
{
	MessageImpl(std::move(text));
}

void SDR::Log::MessageColor(Shared::Color col, std::string&& text)
{
	MessageColorImpl(col, std::move(text));
}

void SDR::Log::Warning(std::string&& text)
{
	WarningImpl(std::move(text));
}
