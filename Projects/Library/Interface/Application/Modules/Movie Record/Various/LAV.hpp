#pragma once

/*
	avutil below will complain about this not being defined.
*/
#define __STDC_CONSTANT_MACROS

extern "C"
{
	#include <libavutil\avutil.h>
	#include <libavutil\imgutils.h>
	#include <libavcodec\avcodec.h>
	#include <libavformat\avformat.h>
}

#undef __STDC_CONSTANT_MACROS

namespace SDR::LAV
{
	struct ScopedFormatContext
	{
		~ScopedFormatContext();

		void Assign(const char* filename);
		
		AVFormatContext* Get() const;
		AVFormatContext* operator->() const;

		AVFormatContext* Context = nullptr;
	};

	struct ScopedAVFrame
	{
		~ScopedAVFrame();

		void Assign(int width, int height, AVPixelFormat format, AVColorSpace colorspace, AVColorRange colorrange);
		
		AVFrame* Get() const;
		AVFrame* operator->() const;

		AVFrame* Frame = nullptr;
	};

	struct ScopedAVDictionary
	{
		~ScopedAVDictionary();

		AVDictionary** Get();
		void Set(const char* key, const char* value, int flags = 0);

		AVDictionary* Options = nullptr;
	};
}
