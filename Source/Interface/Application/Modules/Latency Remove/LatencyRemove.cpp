#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#if 0
namespace
{
	namespace Module_GetClientInterpAmount
	{
		float __fastcall Override(void* thisptr, void* edx);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleStaticAddress<ThisFunction> ThisHook
		{
			"engine.dll", "CClientState_GetClientInterpAmount", Override, 0x100CFE30
		};

		/*
			
		*/
		float __fastcall Override(void* thisptr, void* edx)
		{
			return ThisHook.GetOriginal()(thisptr, edx);
		}
	}

	namespace Module_InterpolateViewpoint
	{
		void __fastcall Override(void* thisptr, void* edx);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleStaticAddress<ThisFunction> ThisHook
		{
			"engine.dll", "CDemoPlayer_InterpolateViewpoint", Override, 0x101EE240
		};

		/*
			
		*/
		void __fastcall Override(void* thisptr, void* edx)
		{
			ThisHook.GetOriginal()(thisptr, edx);
		}
	}
}
#endif
