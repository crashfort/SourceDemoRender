#pragma once
#include <string>
#include <vector>
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Error.hpp>

namespace SDR::ConfigSystem
{
	template <typename NodeType, typename FuncType>
	void MemberLoop(NodeType& node, FuncType callback)
	{
		auto& begin = node.MemberBegin();
		auto& end = node.MemberEnd();

		for (auto it = begin; it != end; ++it)
		{
			callback(it);
		}
	}

	struct ObjectData
	{
		std::string ObjectName;
		std::vector<std::pair<std::string, rapidjson::Value>> Properties;
	};

	ObjectData* FindAndPopulateObject(rapidjson::Document& document, const char* searcher, std::vector<ObjectData>& dest);

	void ResolveInherit(ObjectData* object, const std::vector<ObjectData>& source, rapidjson::Document::AllocatorType& alloc);
	void ResolveSort(ObjectData* object);
}
