#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include "Interface\Application\Modules\Shared\EngineClient.hpp"

namespace
{
	bool IsUnfocused = false;
}

namespace
{
	namespace ModuleEngineInfo
	{
		/*
			The offset of m_bActiveApp in CGame.
		*/
		int ActiveAppOffset;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"Engine_Info",
				[](const char* name, rapidjson::Value& value)
				{
					ActiveAppOffset = value["ActiveAppOffset"].GetInt();
					return true;
				}
			)
		);
	}

	namespace ModuleActivateMouse
	{
		/*
			This is needed because it's responsible for locking the mouse inside the window.
		*/

		namespace Variant0
		{
			void __cdecl NewFunction();

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction()
			{
				if (!IsUnfocused)
				{
					ThisHook.GetOriginal()();
				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"ActivateMouse",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
						value
					);
				}
			)
		);
	}

	namespace ModuleAppActivate
	{
		enum
		{
			VariantCount = 1
		};

		namespace Variant0
		{
			/*
				Structure from Source 2007.
			*/
			struct InputEvent_t
			{
				int m_nType;
				int m_nTick;
				int m_nData;
				int m_nData2;
				int m_nData3;
			};

			void __fastcall NewFunction(void* thisptr, void* edx, const InputEvent_t& event);

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __fastcall NewFunction(void* thisptr, void* edx, const InputEvent_t& event)
			{
				IsUnfocused = event.m_nData == 0;

				auto& isactiveapp = *(bool*)((char*)(thisptr) + ModuleEngineInfo::ActiveAppOffset);

				ThisHook.GetOriginal()(thisptr, edx, event);

				/*
					Deep in the engine somewhere in CEngine::Frame, the logical
					FPS is lowered when the window is unfocused to save performance.
					That also makes the processing slower if you are alt tabbed.
				*/
				if (IsUnfocused)
				{
					isactiveapp = true;
				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"AppActivate",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
						value
					);
				}
			)
		);
	}
}
