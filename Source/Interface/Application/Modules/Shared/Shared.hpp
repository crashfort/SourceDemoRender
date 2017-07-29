#pragma once

namespace SDR::Shared
{
	inline ConVar MakeBool(const char* name, const char* value)
	{
		return ConVar(name, value, FCVAR_NEVER_AS_STRING, "", true, 0, true, 1);
	}

	template <typename T>
	inline ConVar MakeNumber(const char* name, const char* value, T min, T max)
	{
		return ConVar(name, value, FCVAR_NEVER_AS_STRING, "", true, min, true, max);
	}

	template <typename T>
	inline ConVar MakeNumberWithString(const char* name, const char* value, T min, T max)
	{
		return ConVar(name, value, 0, "", true, min, true, max);
	}

	template <typename T>
	inline ConVar MakeNumber(const char* name, const char* value, T min)
	{
		return ConVar(name, value, FCVAR_NEVER_AS_STRING, "", true, min, false, 0);
	}

	inline ConVar MakeString(const char* name, const char* value)
	{
		return ConVar(name, value, 0, "");
	}
}
