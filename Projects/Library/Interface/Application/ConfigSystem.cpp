#include "PrecompiledHeader.hpp"
#include "ConfigSystem.hpp"

SDR::ConfigSystem::ObjectData* SDR::ConfigSystem::FindAndPopulateObject(rapidjson::Document& document, const char* searcher, std::vector<ObjectData>& dest)
{
	MemberLoop(document, [&](rapidjson::Document::MemberIterator it)
	{
		dest.emplace_back();
		auto& curobj = dest.back();

		curobj.ObjectName = it->name.GetString();

		MemberLoop(it->value, [&](rapidjson::Document::MemberIterator data)
		{
			PropertyData newprop;
			newprop.Name = data->name.GetString();
			newprop.Value = std::move(data->value);

			curobj.Properties.emplace_back(std::move(newprop));
		});
	});

	for (auto& obj : dest)
	{
		if (String::EndsWith(searcher, obj.ObjectName.c_str()))
		{
			return &obj;
		}
	}

	return nullptr;
}

void SDR::ConfigSystem::ResolveInherit(ObjectData* object, std::vector<ObjectData>& source, rapidjson::Document::AllocatorType& alloc)
{
	auto foundinherit = false;

	ObjectData::PropertiesType temp;

	for (auto it = object->Properties.begin(); it != object->Properties.end(); ++it)
	{
		if (it->Name == "Inherit")
		{
			foundinherit = true;

			if (!it->Value.IsString())
			{
				Error::Make("SDR: \"%s\" inherit field not a string\n", object->ObjectName.c_str());
			}

			std::string from = it->Value.GetString();

			object->Properties.erase(it);

			for (auto&& sourceobj : source)
			{
				bool foundparent = false;

				if (sourceobj.ObjectName == from)
				{
					foundparent = true;

					for (auto&& sourceprop : sourceobj.Properties)
					{
						bool shouldadd = true;

						for (const auto& destprop : object->Properties)
						{
							if (sourceprop.Name == destprop.Name)
							{
								shouldadd = false;
								break;
							}
						}

						if (shouldadd)
						{
							temp.emplace_back(std::move(sourceprop));
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
					Error::Make("\"%s\" inherit target \"%s\" not found", object->ObjectName.c_str(), from.c_str());
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
			if (prop.Name.empty())
			{
				continue;
			}

			if (prop.Value.IsObject())
			{
				if (prop.Value.HasMember("SortGroup"))
				{
					std::string group = prop.Value["SortGroup"].GetString();

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
		if (rem.Name.empty())
		{
			continue;
		}

		object->Properties.emplace_back(std::move(rem));
	}
}
