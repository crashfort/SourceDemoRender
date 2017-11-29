#include "SDR Extension\Extension.hpp"
#include "SDR Shared\String.hpp"
#include "SDR Shared\Error.hpp"
#include <d3d11.h>

extern "C"
{
	__declspec(dllexport) void __cdecl SDR_Query(SDR::Extension::QueryData& query)
	{
		query.Name = "Sample Extension";
		query.Namespace = "SampleExtension";
		query.Author = "crashfort";
		query.Contact = "https://github.com/crashfort/";
		
		query.Version = 1;
	}

	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::Extension::InitializeData& data)
	{
		SDR::Error::SetPrintFormat("SampleExtension: %s\n");
		SDR::Extension::RedirectLogOutputs(data);
	}

	__declspec(dllexport) bool __cdecl SDR_ConfigHandler(const char* name, const rapidjson::Value& value)
	{
		return false;
	}

	__declspec(dllexport) void __cdecl SDR_Ready(const SDR::Extension::ImportData& data)
	{
		SDR::Log::Message("Hello from sample extension!\n"s);
	}

	__declspec(dllexport) void __cdecl SDR_StartMovie(const SDR::Extension::StartMovieData& data)
	{
		
	}

	__declspec(dllexport) void __cdecl SDR_EndMovie()
	{
		
	}

	__declspec(dllexport) void __cdecl SDR_ModifyFrame(SDR::Extension::ModifyFrameData& data)
	{
		
	}
}
