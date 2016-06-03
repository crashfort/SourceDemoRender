#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	struct TabData
	{
		bool IsUnfocused = false;
	};

	TabData EngineFocusData;

	namespace Module_VGUI_ActivateMouse
	{
		#define CALLING __cdecl
		#define RETURNTYPE void
		#define PARAMETERS
		#define THISFUNCTION RETURNTYPE(CALLING*)(PARAMETERS)
		#define CALLORIGINAL reinterpret_cast<THISFUNCTION>(ThisHook.GetOriginalFunction())

		/*
			0x10216D80 static IDA address June 3 2016
		*/
		auto Pattern = SDR_PATTERN("\x83\x3D\x00\x00\x00\x00\x00\x74\x4F\x8B\x0D\x00\x00\x00\x00\x8B\x01\x8B\x40\x40\xFF\xD0");
		auto Mask = "xx?????xxxx????xxxxxxx";

		/*
			This is needed because it's responsible for locking the mouse inside the window
		*/
		SDR::HookModuleMask<THISFUNCTION> ThisHook
		{
			"engine.dll", "VGUI_ActivateMouse",
			[](PARAMETERS)
			{
				if (!EngineFocusData.IsUnfocused)
				{
					CALLORIGINAL();
				}

			}, Pattern, Mask
		};
	}

	namespace Module_AppActivate
	{
		/*
			Structure from Source 2007
		*/
		struct InputEvent_t
		{
			int m_nType; // Type of the event (see InputEventType_t)
			int m_nTick; // Tick on which the event occurred
			int m_nData; // Generic 32-bit data, what it contains depends on the event
			int m_nData2; // Generic 32-bit data, what it contains depends on the event
			int m_nData3; // Generic 32-bit data, what it contains depends on the event
		};

		#define CALLING __fastcall
		#define RETURNTYPE void
		#define PARAMETERS void* thisptr, void* edx, const InputEvent_t& event
		#define THISFUNCTION RETURNTYPE(CALLING*)(PARAMETERS)
		#define CALLORIGINAL reinterpret_cast<THISFUNCTION>(ThisHook.GetOriginalFunction())

		/*
			0x102013C0 static IDA address June 3 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x8B\x45\x08\x83\x78\x08\x00\x0F\x95\xC0\x0F\xB6\xC0\x89\x45\x08\x5D\xE9\x00\x00\x00\x00");
		auto Mask = "xxxxxxxxxxxxxxxxxxxxx????";

		SDR::HookModuleMask<THISFUNCTION> ThisHook
		{
			"engine.dll", "CGame_AppActivate",
			[](PARAMETERS)
			{
				auto& interfaces = SDR::GetEngineInterfaces();

				if (interfaces.EngineClient->IsPlayingDemo())
				{
					EngineFocusData.IsUnfocused = event.m_nData == 0;

					/*
						52 is the offset of m_bActiveApp in CGame
					*/
					auto& isactiveapp = *(bool*)((char*)(thisptr) + 52);

					CALLORIGINAL(thisptr, edx, event);

					/*
						Deep in the engine somewhere in CEngine::Frame, the logical
						FPS is lowered when the window is unfocused to save performance.
						That also makes the processing slower if you are alt tabbed.
					*/
					if (EngineFocusData.IsUnfocused)
					{
						isactiveapp = true;
					}
				}

				else
				{
					CALLORIGINAL(thisptr, edx, event);
				}

			}, Pattern, Mask
		};
	}
}
