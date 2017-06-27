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

	namespace ModuleOutputDebugString
	{
		#pragma region Init

		void __stdcall Override(LPCSTR outputstring);

		using ThisFunction = decltype(OutputDebugStringA)*;

		SDR::HookModule<ThisFunction> ThisHook;

		SDR::PluginStartupFunctionAdder Adder
		(
			"OutputDebugString Patch",
			[]()
			{
				auto res = MH_CreateHookApiEx
				(
					L"kernel32.dll",
					"OutputDebugStringA",
					Override,
					&ThisHook.OriginalFunction,
					&ThisHook.TargetFunction
				);

				return true;
			}
		);

		#pragma endregion

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

	namespace ModuleActivateMouse
	{
		#pragma region Init

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"ActivateMouse",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		/*
			This is needed because it's responsible
			for locking the mouse inside the window
		*/
		void __cdecl Override()
		{
			if (!EngineFocusData.IsUnfocused)
			{
				ThisHook.GetOriginal()();
			}
		}
	}

	namespace ModuleAppActivate
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

		#pragma region Init

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			const InputEvent_t& event
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"AppActivate",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			const InputEvent_t& event
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
