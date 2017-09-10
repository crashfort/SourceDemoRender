#pragma once
#include "rapidjson\document.h"
#include "File.hpp"
#include "Error.hpp"

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

	inline rapidjson::Document FromFile(const std::string& name)
	{
		return FromFile(name.c_str());
	}

	inline auto GetIterator(const rapidjson::Value& value, const char* name)
	{
		auto it = value.FindMember(name);

		if (it == value.MemberEnd())
		{
			SDR::Error::Make("Member Json value \"%s\" not found", name);
		}

		return it;
	}

	inline auto GetInt(const rapidjson::Value& value, const char* name)
	{
		auto it = GetIterator(value, name);

		if (!it->value.IsInt())
		{
			SDR::Error::Make("Member Json value \"%s\" not an int", name);
		}

		return it->value.GetInt();
	}

	inline auto GetString(const rapidjson::Value& value, const char* name)
	{
		auto it = GetIterator(value, name);

		if (!it->value.IsString())
		{
			SDR::Error::Make("Member Json value \"%s\" not a string", name);
		}

		return it->value.GetString();
	}
}
