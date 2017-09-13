#include "SDR Shared\Error.hpp"
#include <vector>

namespace
{
	const char* PrintFormat = "%s\n";
	std::vector<std::string> ContextStack;
}

void SDR::Error::SetPrintFormat(const char* format)
{
	PrintFormat = format;
}

void SDR::Error::Print(const Exception& error)
{
	auto showstack = !ContextStack.empty();

	if (showstack)
	{
		auto tabsize = 4;
		std::string tabs;

		for (const auto& str : ContextStack)
		{
			Log::Warning("%s\"%s\"\n%s{\n", tabs.c_str(), str.c_str(), tabs.c_str());
			tabs.append(tabsize, ' ');
		}

		Log::Warning("%s%s\n", tabs.c_str(), error.Description.c_str());

		for (const auto& str : ContextStack)
		{
			tabs.resize(tabs.size() - tabsize);
			Log::Warning("%s}\n", tabs.c_str());
		}
	}

	else
	{
		Log::Warning(PrintFormat, error.Description.c_str());
	}
}

SDR::Error::ScopedContext::ScopedContext(std::string&& str)
{
	ContextStack.emplace_back(std::move(str));
}

SDR::Error::ScopedContext::~ScopedContext()
{
	ContextStack.pop_back();
}
