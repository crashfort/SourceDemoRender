#pragma once
#include "rapidjson\document.h"
#include "SDR Shared\Log.hpp"
#include <d3d11.h>

namespace SDR::Extension
{
	struct QueryData
	{
		const char* Name;
		const char* Namespace;
		const char* Author;
		const char* Contact;
		int Version;
	};

	struct InitializeData
	{
		Log::LogFunctionType Message;
		Log::LogFunctionColorType MessageColor;
		Log::LogFunctionType Warning;
	};

	struct ConsoleData
	{
		uint32_t(*MakeBool)(const char* name, const char* value);
		uint32_t(*MakeNumber)(const char* name, const char* value);
		uint32_t(*MakeNumberMin)(const char* name, const char* value, float min);
		uint32_t(*MakeNumberMinMax)(const char* name, const char* value, float min, float max);
		uint32_t(*MakeNumberMinMaxString)(const char* name, const char* value, float min, float max);
		uint32_t(*MakeString)(const char* name, const char* value);

		bool(*GetBool)(uint32_t key);
		int(*GetInt)(uint32_t key);
		float(*GetFloat)(uint32_t key);
		const char*(*GetString)(uint32_t key);
	};

	struct ModifyFrameData
	{
		ID3D11DeviceContext* Context;
		ID3D11UnorderedAccessView* GameFrameUAV;
		ID3D11Buffer* ConstantBuffer;

		int ThreadGroupsX;
		int ThreadGroupsY;
	};

	inline void RedirectLogOutputs(InitializeData* data)
	{
		SDR::Log::SetMessageFunction(data->Message);
		SDR::Log::SetMessageColorFunction(data->MessageColor);
		SDR::Log::SetWarningFunction(data->Warning);
	}

	namespace ExportTypes
	{
		using SDR_Query = void(__cdecl*)(QueryData* query);
		using SDR_Initialize = void(__cdecl*)(InitializeData* data);
		using SDR_CallHandlers = bool(__cdecl*)(const char* name, const rapidjson::Value& value);
		using SDR_Ready = void(__cdecl*)(const SDR::Extension::ConsoleData& data);
		using SDR_StartMovie = void(__cdecl*)(ID3D11Device* device, int width, int height);
		using SDR_EndMovie = void(__cdecl*)();
		using SDR_ModifyFrame = void(*)(ModifyFrameData& data);
	}
}
