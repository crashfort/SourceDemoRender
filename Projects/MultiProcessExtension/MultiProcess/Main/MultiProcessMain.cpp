#include <SDR Extension\Extension.hpp>
#include <SDR Shared\Error.hpp>
#include <SDR Shared\Hooking.hpp>

extern "C"
{
	__declspec(dllexport) void __cdecl SDR_Query(SDR::Extension::QueryData& query)
	{
		query.Name = "Multi Process";
		query.Namespace = "MultiProcess";
		query.Author = "crashfort";
		query.Contact = "https://github.com/crashfort/";
		
		query.Version = 2;
	}

	__declspec(dllexport) void __cdecl SDR_Initialize(const SDR::Extension::InitializeData& data)
	{
		SDR::Error::SetPrintFormat("MultiProcess: %s\n");
		SDR::Extension::RedirectLogOutputs(data);


		auto mutex = OpenMutexA(MUTEX_ALL_ACCESS, false, "hl2_singleton_mutex");

		if (mutex)
		{
			ReleaseMutex(mutex);
			CloseHandle(mutex);
		}
	}
}
