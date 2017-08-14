#include "SDR Shared\Error.hpp"

namespace
{
	const char* PrintFormat = "%s\n";
}

void SDR::Error::SetPrintFormat(const char* format)
{
	PrintFormat = format;
}

void SDR::Error::Print(const Exception& error)
{
	Log::Warning(PrintFormat, error.Description.c_str());
}
