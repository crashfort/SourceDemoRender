#pragma once
#include "String.hpp"

namespace SDR::Table
{
	template <typename Table, typename Variable>
	bool LinkToVariable(const char* key, Table& table, Variable& variable)
	{
		for (const auto& entry : table)
		{
			if (SDR::String::IsEqual(key, entry.first))
			{
				variable = std::move(entry.second);
				return true;
			}
		}

		return false;
	}
}
