#pragma once
#include "rapidjson\document.h"
#include "File.hpp"

namespace SDR::Json
{
	inline rapidjson::Document FromFile(const char* name)
	{
		SDR::File::ScopedFile file(name, "rb");

		auto strdata = file.ReadString();

		rapidjson::Document document;
		document.Parse(strdata.c_str());

		return document;
	}
}
