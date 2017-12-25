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

	namespace Colors
	{
		enum
		{
			White = RGB(200, 200, 200),
			Dark = RGB(150, 150, 150),
			Red = RGB(252, 62, 54),
			Green = RGB(100, 255, 100),
			String = RGB(214, 157, 133),
			Number = RGB(181, 206, 168),
		};
	}

	struct AddMessageData
	{
		char Text[1024];
	};

	namespace Messages
	{
		enum
		{
			AddMessage = WM_APP + 1
		};
	}
}
