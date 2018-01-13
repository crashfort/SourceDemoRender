#pragma once
#include <string>
#include <vector>
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Error.hpp>

namespace SDR::ConfigSystem
{
	struct PropertyData
	{
		std::string Name;
		rapidjson::Value Value;
	};

	struct ObjectData
	{
		using PropertiesType = std::vector<PropertyData>;

		std::string ObjectName;
		PropertiesType Properties;
	};

	ObjectData* FindAndPopulateObject(rapidjson::Document& document, const char* searcher, std::vector<ObjectData>& dest);

	void ResolveInherit(ObjectData* object, std::vector<ObjectData>& source);
	void ResolveSort(ObjectData* object);
}
