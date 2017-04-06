#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	namespace Variables
	{
		ConVar SuppressDebugLog
		(
			"sdr_game_suppressdebug", "0", 0,
			"Disable slow game debug output logging"
		);
	}

	namespace Module_OutputDebugString
	{
		void __stdcall Override(LPCSTR outputstring);

		using ThisFunction = decltype(OutputDebugStringA)*;

		SDR::HookModuleAPI<ThisFunction> ThisHook
		{
			"kernel32.dll", "OutputDebugString", "OutputDebugStringA", Override
		};

		void __stdcall Override(LPCSTR outputstring)
		{
			if (!Variables::SuppressDebugLog.GetBool())
			{
				ThisHook.GetOriginal()(outputstring);
			}
		}
	}

	struct TabData
	{
		bool IsUnfocused = false;
	};

	TabData EngineFocusData;

	namespace Module_ActivateMouse
	{
		/*
			0x10216D80 static IDA address June 3 2016
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x83\x3D\x00\x00\x00\x00\x00\x74\x4F\x8B\x0D\x00\x00\x00\x00\x8B\x01\x8B\x40\x40\xFF\xD0"
		);

		auto Mask = "xx?????xxxx????xxxxxxx";

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "ActivateMouse", Override, Pattern, Mask
		};

		/*
			This is needed because it's responsible for locking the mouse inside the window
		*/
		void __cdecl Override()
		{
			if (!EngineFocusData.IsUnfocused)
			{
				ThisHook.GetOriginal()();
			}
		}
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

		/*
			0x102013C0 static IDA address June 3 2016
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x8B\x45\x08\x83\x78\x08\x00\x0F\x95\xC0"
			"\x0F\xB6\xC0\x89\x45\x08\x5D\xE9\x00\x00\x00\x00"
		);

		auto Mask = "xxxxxxxxxxxxxxxxxxxxx????";

		void __fastcall Override
		(
			void* thisptr, void* edx, const InputEvent_t& event
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "AppActivate", Override, Pattern, Mask
		};

		void __fastcall Override
		(
			void* thisptr, void* edx, const InputEvent_t& event
		)
		{
			auto& interfaces = SDR::GetEngineInterfaces();

			if (interfaces.EngineClient->IsPlayingDemo())
			{
				EngineFocusData.IsUnfocused = event.m_nData == 0;

				/*
					52 is the offset of m_bActiveApp in CGame
				*/
				auto& isactiveapp = *(bool*)((char*)(thisptr) + 52);

				ThisHook.GetOriginal()(thisptr, edx, event);

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
				ThisHook.GetOriginal()(thisptr, edx, event);
			}
		}
	}
}
