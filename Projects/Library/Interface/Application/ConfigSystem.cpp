#include "PrecompiledHeader.hpp"
#include "ConfigSystem.hpp"

SDR::ConfigSystem::ObjectData* SDR::ConfigSystem::FindAndPopulateObject(rapidjson::Document& document, const char* searcher, std::vector<ObjectData>& dest)
{
	SDR::ConfigSystem::MemberLoop(document, [&](rapidjson::Document::MemberIterator it)
	{
		dest.emplace_back();
		auto& curobj = dest.back();

		curobj.ObjectName = it->name.GetString();

		SDR::ConfigSystem::MemberLoop(it->value, [&](rapidjson::Document::MemberIterator data)
		{
			curobj.Properties.emplace_back(data->name.GetString(), std::move(data->value));
		});
	});

	for (auto& obj : dest)
	{
		if (SDR::String::EndsWith(searcher, obj.ObjectName.c_str()))
		{
			return &obj;
		}
	}

	return nullptr;
}

void SDR::ConfigSystem::ResolveInherit(ObjectData* object, const std::vector<ObjectData>& source, rapidjson::Document::AllocatorType& alloc)
{
	auto foundinherit = false;

	std::vector<std::pair<std::string, rapidjson::Value>> temp;

	for (auto it = object->Properties.begin(); it != object->Properties.end(); ++it)
	{
		if (it->first == "Inherit")
		{
			foundinherit = true;

			if (!it->second.IsString())
			{
				SDR::Error::Make("SDR: \"%s\" inherit field not a string\n", object->ObjectName.c_str());
			}

			std::string from = it->second.GetString();

			object->Properties.erase(it);

			for (const auto& sourceobj : source)
			{
				bool foundparent = false;

				if (sourceobj.ObjectName == from)
				{
					foundparent = true;

					for (const auto& sourceprop : sourceobj.Properties)
					{
						bool shouldadd = true;

						for (const auto& destprop : object->Properties)
						{
							if (sourceprop.first == destprop.first)
							{
								shouldadd = false;
								break;
							}
						}

						if (shouldadd)
						{
							temp.emplace_back(std::make_pair(sourceprop.first, rapidjson::Value(sourceprop.second, alloc)));
						}
					}

					for (auto&& orig : object->Properties)
					{
						temp.emplace_back(std::move(orig));
					}

					object->Properties = std::move(temp);

					break;
				}

				if (!foundparent)
				{
					SDR::Error::Make("\"%s\" inherit target \"%s\" not found", object->ObjectName.c_str(), from.c_str());
				}
			}

			break;
		}
	}

	if (foundinherit)
	{
		ResolveInherit(object, source, alloc);
	}
}

void SDR::ConfigSystem::ResolveSort(ObjectData* object)
{
	auto temp = std::move(object->Properties);

	auto addgroup = [&](const char* name)
	{
		for (auto&& prop : temp)
		{
			if (prop.first.empty())
			{
				continue;
			}

			if (prop.second.IsObject())
			{
				if (prop.second.HasMember("SortGroup"))
				{
					std::string group = prop.second["SortGroup"].GetString();

					if (group == name)
					{
						object->Properties.emplace_back(std::move(prop));
					}
				}
			}
		}
	};

	addgroup("Pointer");
	addgroup("Info");
	addgroup("Function");
	addgroup("User1");
	addgroup("User2");
	addgroup("User3");
	addgroup("User4");

	for (auto&& rem : temp)
	{
		if (rem.first.empty())
		{
			continue;
		}

		object->Properties.emplace_back(std::move(rem));
	}
}
