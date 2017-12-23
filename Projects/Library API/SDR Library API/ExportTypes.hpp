#pragma once
#include <cstdint>
#include <cstdio>
#include <wrl.h>
#include <SDR Shared\String.hpp>

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

	inline std::string CreatePipeName(StageType stage)
	{
		auto stagenum = (uint32_t)stage;
		return String::Format(R"(\\.\pipe\sdr_loader_pipe_%d)", stagenum);
	}

	inline std::string CreateEventSuccessName(StageType stage)
	{
		auto stagenum = (uint32_t)stage;
		return String::Format("SDR_LOADER_SUCCESS_%d", stagenum);
	}

	inline std::string CreateEventFailureName(StageType stage)
	{
		auto stagenum = (uint32_t)stage;
		return String::Format("SDR_LOADER_FAIL_%d", stagenum);
	}

	using SDR_LibraryVersion = int(__cdecl*)();
	using SDR_Initialize = void(__cdecl*)(const char* respath, const char* gamepath);
}
