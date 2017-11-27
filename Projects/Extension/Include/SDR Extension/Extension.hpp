#pragma once
#include "rapidjson\document.h"
#include "SDR Shared\Log.hpp"
#include <d3d11.h>

namespace SDR::Extension
{
	struct QueryData
	{
		const char* Name;
		const char* Author;
		const char* Contact;
		int Version;
	};

	struct ReadyData
	{
		Log::LogFunctionType Message;
		Log::LogFunctionColorType MessageColor;
		Log::LogFunctionType Warning;
		Log::LogFunctionType MakeError;
	};

	namespace ExportTypes
	{
		using SDR_Query = void(__cdecl*)(QueryData* query);
		using SDR_CallHandlers = bool(__cdecl*)(const char* name, const rapidjson::Value& value);
		using SDR_Ready = void(*)(ReadyData* data);
		using SDR_ModifyFrame = void(*)(ID3D11DeviceContext* context);
	}
}
