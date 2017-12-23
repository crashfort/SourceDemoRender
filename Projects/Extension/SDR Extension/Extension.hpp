#pragma once
#include <rapidjson\document.h>
#include <SDR Shared\Log.hpp>
#include <SDR Shared\ConsoleTypes.hpp>
#include <d3d9.h>
#include <d3d11.h>

namespace SDR::Extension
{
	/*
		Sent to SDR_Query for the extension to fill in.
	*/
	struct QueryData
	{
		/*
			Display name of this extension.
		*/
		const char* Name;

		/*
			An extension will only receieve calls to "SDR_ConfigHandler" if an object
			in "ExtensionConfig.json" is prefixed with this namespace.
		*/
		const char* Namespace;

		/*
			Collection of authors that developed this extension.
		*/
		const char* Author;

		/*
			Location of where support is to be seeked.
		*/
		const char* Contact;
		
		int Version;
	};

	/*
		Sent to SDR_Initialize, contains the functions from the main library.
		Use "RedirectLogOutputs" below to redirect the SDR::Log functions to the game console.
	*/
	struct InitializeData
	{
		/*
			Normal console message.
		*/
		Log::LogFunctionType Message;

		/*
			Same as "Message", but takes a color.
		*/
		Log::LogFunctionColorType MessageColor;

		/*
			Console message with a red color.
		*/
		Log::LogFunctionType Warning;
	};

	/*
		Sent to SDR_Ready, contains functions that interact with the game.
	*/
	struct ImportData
	{
		/*
			Make console variables. Returns a key which is used to reference it at a later time.
		*/
		uint32_t(*MakeBool)(const char* name, const char* value);
		uint32_t(*MakeNumber)(const char* name, const char* value);
		uint32_t(*MakeNumberMin)(const char* name, const char* value, float min);
		uint32_t(*MakeNumberMinMax)(const char* name, const char* value, float min, float max);
		uint32_t(*MakeNumberMinMaxString)(const char* name, const char* value, float min, float max);
		uint32_t(*MakeString)(const char* name, const char* value);

		/*
			Use key from above functions to get its values.
		*/
		bool(*GetBool)(uint32_t key);
		int(*GetInt)(uint32_t key);
		float(*GetFloat)(uint32_t key);
		const char*(*GetString)(uint32_t key);

		/*
			Get values from other console variables by name.
		*/
		bool(*GetExternalBool)(const char* name);
		int(*GetExternalInt)(const char* name);
		float(*GetExternalFloat)(const char* name);
		const char*(*GetExternalString)(const char* name);

		/*
			Command callbacks that are executed from the game console.
		*/
		void(*MakeCommandVoid)(const char* name, Console::Types::CommandCallbackVoidType func);
		void(*MakeCommandArgs)(const char* name, Console::Types::CommandCallbackArgsType func);

		/*
			For command callbacks that take arguments, use these on the parameter.
		*/
		int(*GetCommandArgumentCount)(const void* ptr);
		const char*(*GetCommandArgumentAt)(const void* ptr, int index);
		const char*(*GetCommandArgumentFull)(const void* ptr);

		/*
			Returns current time in high resolution.
		*/
		double(*GetTimeNow)();

		/*
			From module MovieRecord, returns true if video frames are being processed.
		*/
		bool(*IsRecordingVideo)();

		/*
			Returns engine D3D9 device.
		*/
		IDirect3DDevice9Ex*(*GetD3D9Device)();

		/*
			Unique key that is used to reference this extension to the main library.
		*/
		uint32_t ExtensionKey;

		/*
			Returns number of active extensions. The returned value can be used as the
			upper bound for a loop, with each index being an extension key.
		*/
		size_t(*GetExtensionCount)();
		
		/*
			Returns the module for the given extension.
		*/
		HMODULE(*GetExtensionModule)(uint32_t key);

		/*
			Returns the file name of the given extension. The returned string is only
			the name with extension.
		*/
		const char*(*GetExtensionFileName)(uint32_t key);
	};

	/*
		Sent to SDR_StartMovie, contains active D3D11 device and other video parameters.
	*/
	struct StartMovieData
	{
		/*
			The device that the main library is currently using.
		*/
		ID3D11Device* Device;
		
		int Width;
		int Height;

		/*
			The video frame rate.
		*/
		int FrameRate;

		/*
			The game engine frame rate.
		*/
		int HostFrameRate;

		/*
			Value that is being used for every video frame.
		*/
		double TimePerFrame;

		/*
			If sampling is enabled, this will not be the same as "TimePerFrame", but rather
			1.0 / HostFrameRate.
		*/
		double TimePerSample;
	};

	/*
		Sent to SDR_NewVideoFrame when a new video frame is ready but before any conversion.
	*/
	struct NewVideoFrameData
	{
		/*
			The rendering context currently used by the main library.
		*/
		ID3D11DeviceContext* Context;

		/*
			Shader resources for the active video frame.
			In shader code it should use the "WorkBufferData" structure in project "Shader Definitions".

			UAV:
			RWStructuredBuffer<WorkBufferData> WorkBufferUAV : register(u0)

			SRV:
			StructuredBuffer<WorkBufferData> WorkBufferSRV : register(t0)
		*/
		ID3D11UnorderedAccessView* GameFrameUAV;
		ID3D11ShaderResourceView* GameFrameSRV;

		/*
			This buffer should be bound to any activity with above resources. This contains the dimensions of the video frame.
			It should be bound to slot 0.
		*/
		ID3D11Buffer* ConstantBuffer;

		/*
			How many thread groups that should be dispatched to a compute shader.
			These values must be used if the compute shader is using "ThreadsX" and "ThreadsY" in project "Shader Definitions".
		*/
		int ThreadGroupsX;
		int ThreadGroupsY;
	};

	/*
		Within "SDR_Initialize", redirect the SDR::Log functions to the game console.
	*/
	inline void RedirectLogOutputs(const InitializeData& data)
	{
		SDR::Log::SetMessageFunction(data.Message);
		SDR::Log::SetMessageColorFunction(data.MessageColor);
		SDR::Log::SetWarningFunction(data.Warning);
	}

	/*
		These are the correct signatures that extensions should use.
	*/
	namespace ExportTypes
	{
		/*
			Fill in information about your extension.
		*/
		using SDR_Query = void(__cdecl*)(QueryData& query);

		/*
			Called directly after "SDR_Query" to set up initial extension state.
		*/
		using SDR_Initialize = void(__cdecl*)(const InitializeData& data);

		/*
			For every matching namespace entry in ExtensionConfig.json, this is the callback.
			Return true if the handler was found by name.

			Optional.
		*/
		using SDR_ConfigHandler = bool(__cdecl*)(const char* name, const rapidjson::Value& value);

		/*
			Called when SDR is fully loaded. The parameter can be stored off for future use,
			it's the gateway for communicating with the main library.

			Optional.
		*/
		using SDR_Ready = void(__cdecl*)(const SDR::Extension::ImportData& data);

		/*
			Called after the main library handles the start movie command.

			Optional.
		*/
		using SDR_StartMovie = void(__cdecl*)(const StartMovieData& data);

		/*
			Called after all processing by the main library is done.

			Optional.
		*/
		using SDR_EndMovie = void(__cdecl*)();

		/*
			Called when a new video frame is ready. The content can be manipulated or viewed before it gets written.
			The execution order of the extensions is important here.

			Optional.
		*/
		using SDR_NewVideoFrame = void(__cdecl*)(const NewVideoFrameData& data);
	}
}
