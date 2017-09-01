#pragma once
#include <cstdint>
#include <cstdio>
#include <wrl.h>

namespace SDR::API
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

		ScopedHandle Pipe;
		ScopedHandle EventSuccess;
		ScopedHandle EventFailure;
	};

	template <size_t Size>
	void CreatePipeName(char(&buffer)[Size], StageType stage)
	{
		auto stagenum = (uint32_t)stage;
		sprintf_s(buffer, R"(\\.\pipe\sdr_loader_pipe_%d)", stagenum);
	}

	template <size_t Size>
	void CreateEventSuccessName(char(&buffer)[Size], StageType stage)
	{
		auto stagenum = (uint32_t)stage;
		sprintf_s(buffer, "SDR_LOADER_SUCCESS_%d", stagenum);
	}

	template <size_t Size>
	void CreateEventFailureName(char(&buffer)[Size], StageType stage)
	{
		auto stagenum = (uint32_t)stage;
		sprintf_s(buffer, "SDR_LOADER_FAIL_%d", stagenum);
	}

	using SDR_LibraryVersion = int(__cdecl*)();
	using SDR_Initialize = void(__cdecl*)(const char* path, const char* game);
}
