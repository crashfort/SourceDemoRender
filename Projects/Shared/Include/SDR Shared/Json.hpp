#pragma once
#include "rapidjson\document.h"
#include "File.hpp"

namespace SDR::Json
{
	inline rapidjson::Document FromFile(const char* name)
	{
		SDR::File::ScopedFile file;
		file.Assign(name, "rb");

		auto data = file.ReadAll();
		std::string strdata((const char*)data.data(), data.size());

		rapidjson::Document document;
		document.Parse(strdata.c_str());

		return document;
	}
}
