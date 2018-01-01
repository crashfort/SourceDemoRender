#include <SDR Shared\Log.hpp>

namespace
{
	void MessageDefault(const char* text)
	{
		printf_s(text);
	}

	void MessageColorDefault(SDR::Shared::Color col, const char* text)
	{
		printf_s(text);
	}
	
	void WarningDefault(const char* text)
	{
		printf_s(text);
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

void SDR::Log::Message(const char* text)
{
	MessageImpl(text);
}

void SDR::Log::MessageColor(Shared::Color col, const char* text)
{
	MessageColorImpl(col, text);
}

void SDR::Log::Warning(const char* text)
{
	WarningImpl(text);
}
