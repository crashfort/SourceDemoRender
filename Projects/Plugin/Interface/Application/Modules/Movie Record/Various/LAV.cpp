#include "LAV.hpp"
#include "SDR Shared\Error.hpp"

SDR::LAV::ScopedFormatContext::~ScopedFormatContext()
{
	if (Context)
	{
		if (!(Context->oformat->flags & AVFMT_NOFILE))
		{
			avio_close(Context->pb);
		}

		avformat_free_context(Context);
	}
}

void SDR::LAV::ScopedFormatContext::Assign(const char* filename)
{
	Error::LAV::ThrowIfFailed
	(
		avformat_alloc_output_context2(&Context, nullptr, nullptr, filename),
		"Could not allocate output context for \"%s\"", filename
	);
}

AVFormatContext* SDR::LAV::ScopedFormatContext::Get() const
{
	return Context;
}

AVFormatContext* SDR::LAV::ScopedFormatContext::operator->() const
{
	return Get();
}

SDR::LAV::ScopedAVFrame::~ScopedAVFrame()
{
	if (Frame)
	{
		av_frame_free(&Frame);
	}
}

void SDR::LAV::ScopedAVFrame::Assign(int width, int height, AVPixelFormat format, AVColorSpace colorspace, AVColorRange colorrange)
{
	Frame = av_frame_alloc();

	Error::ThrowIfNull(Frame, "Could not allocate video frame"s);

	Frame->format = format;
	Frame->width = width;
	Frame->height = height;
	Frame->colorspace = colorspace;
	Frame->color_range = colorrange;

	av_frame_get_buffer(Frame, 32);

	Frame->pts = 0;
}

AVFrame* SDR::LAV::ScopedAVFrame::Get() const
{
	return Frame;
}

AVFrame* SDR::LAV::ScopedAVFrame::operator->() const
{
	return Get();
}

SDR::LAV::ScopedAVDictionary::~ScopedAVDictionary()
{
	av_dict_free(&Options);
}

AVDictionary** SDR::LAV::ScopedAVDictionary::Get()
{
	return &Options;
}

void SDR::LAV::ScopedAVDictionary::Set(const char* key, const char* value, int flags)
{
	Error::LAV::ThrowIfFailed
	(
		av_dict_set(Get(), key, value, flags),
		"Could not set dictionary value { \"%s\" = \"%s\" }", key, value
	);
}
