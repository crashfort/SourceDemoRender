#pragma once
#include <cstdint>
#include <cstdio>
#include <wrl.h>

namespace SDR::LauncherCLI
{
	namespace Load
	{
		enum class StageType : uint32_t
		{
			Initialize,
			Load,
		};

		struct ShadowState
		{
			using ScopedHandle = Microsoft::WRL::Wrappers::HandleT
			<
				Microsoft::WRL::Wrappers::HandleTraits::HANDLENullTraits
			>;

			ScopedHandle EventSuccess;
			ScopedHandle EventFailure;
		};

		inline std::string CreateEventSuccessName(StageType stage)
		{
			auto stagenum = (uint32_t)stage;
			return String::Format("SDR_LAUNCHERAPI_SUCCESS_%d", stagenum);
		}

		inline std::string CreateEventFailureName(StageType stage)
		{
			auto stagenum = (uint32_t)stage;
			return String::Format("SDR_LAUNCHERAPI_FAIL_%d", stagenum);
		}
	}

	namespace Message
	{
		namespace Colors
		{
			enum
			{
				White = RGB(220, 220, 220),
				Red = RGB(255, 100, 100)
			};
		}

		struct AddMessageData
		{
			uint32_t Color;
			char Text[1024];
		};

		namespace Types
		{
			enum
			{
				AddMessage = WM_APP + 1
			};
		}
	}
}
