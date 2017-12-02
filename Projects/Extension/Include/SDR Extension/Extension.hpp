#pragma once
#include "rapidjson\document.h"
#include <SDR Shared\Log.hpp>
#include <SDR Shared\ConsoleTypes.hpp>
#include <d3d9.h>
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

	struct ImportData
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

		bool(*GetExternalBool)(const char* name);
		int(*GetExternalInt)(const char* name);
		float(*GetExternalFloat)(const char* name);
		const char*(*GetExternalString)(const char* name);

		void(*MakeCommandVoid)(const char* name, Console::Types::CommandCallbackVoidType func);
		void(*MakeCommandArgs)(const char* name, Console::Types::CommandCallbackArgsType func);

		int(*GetCommandArgumentCount)(const void* ptr);
		const char*(*GetCommandArgumentAt)(const void* ptr, int index);
		const char*(*GetCommandArgumentFull)(const void* ptr);

		double(*GetTimeNow)();

		bool(*IsRecordingVideo)();

		IDirect3DDevice9Ex*(*GetD3D9Device)();
	};

	struct StartMovieData
	{
		ID3D11Device* Device;
		
		int Width;
		int Height;
		int FrameRate;
		int HostFrameRate;

		double TimePerFrame;
		double TimePerSample;
	};

	struct NewVideoFrameData
	{
		ID3D11DeviceContext* Context;
		ID3D11UnorderedAccessView* GameFrameUAV;
		ID3D11ShaderResourceView* GameFrameSRV;
		ID3D11Buffer* ConstantBuffer;

		int ThreadGroupsX;
		int ThreadGroupsY;
	};

	inline void RedirectLogOutputs(const InitializeData& data)
	{
		SDR::Log::SetMessageFunction(data.Message);
		SDR::Log::SetMessageColorFunction(data.MessageColor);
		SDR::Log::SetWarningFunction(data.Warning);
	}

	namespace ExportTypes
	{
		using SDR_Query = void(__cdecl*)(QueryData& query);
		using SDR_Initialize = void(__cdecl*)(const InitializeData& data);
		using SDR_ConfigHandler = bool(__cdecl*)(const char* name, const rapidjson::Value& value);
		using SDR_Ready = void(__cdecl*)(const SDR::Extension::ImportData& data);
		using SDR_StartMovie = void(__cdecl*)(const StartMovieData& data);
		using SDR_EndMovie = void(__cdecl*)();
		using SDR_NewVideoFrame = void(*)(NewVideoFrameData& data);
	}
}
