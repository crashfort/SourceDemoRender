#include "PrecompiledHeader.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	switch (reason)
	{
		case DLL_PROCESS_ATTACH:
		{
			break;
		}

		case DLL_PROCESS_DETACH:
		{
			break;
		}
	}

	return true;
}
