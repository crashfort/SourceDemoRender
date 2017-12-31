#pragma once
#include <SDR Extension\Extension.hpp>
#include <d2d1_3.h>
#include <d2d1_3helper.h>
#include <dwrite_3.h>

namespace SDR::Direct2DContext
{
	struct StartMovieData
	{
		SDR::Extension::StartMovieData Original;

		ID2D1DeviceContext* Context;
		IDWriteFactory* DirectWriteFactory;
	};

	struct NewVideoFrameData
	{
		SDR::Extension::NewVideoFrameData Original;

		ID2D1DeviceContext* Context;
	};

	using Direct2DContext_StartMovie = void(__cdecl*)(const StartMovieData& data);
	using Direct2DContext_NewVideoFrame = void(__cdecl*)(const NewVideoFrameData& data);
}
