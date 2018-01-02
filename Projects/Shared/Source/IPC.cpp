#include <SDR Shared\IPC.hpp>

HANDLE SDR::IPC::WaitForOne(std::initializer_list<HANDLE> handles)
{
	auto waitres = WaitForMultipleObjects(handles.size(), handles.begin(), false, INFINITE);

	if (waitres == WAIT_FAILED)
	{
		SDR::Error::Microsoft::ThrowLastError("Could not wait for event");
	}

	auto maxwait = WAIT_OBJECT_0 + handles.size();

	if (waitres < maxwait)
	{
		auto index = waitres - WAIT_OBJECT_0;
		return *(handles.begin() + index);
	}

	SDR::Error::Make("Unmatched event index"s);
}
