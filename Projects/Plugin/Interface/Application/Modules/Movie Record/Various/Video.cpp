#include "Video.hpp"
#include "SDR Shared\Error.hpp"
#include "Profile.hpp"

void SDR::Video::Writer::OpenFileForWrite(const char* path)
{
	FormatContext.Assign(path);

	if ((FormatContext->oformat->flags & AVFMT_NOFILE) == 0)
	{
		SDR::Error::LAV::ThrowIfFailed
		(
			avio_open(&FormatContext->pb, path, AVIO_FLAG_WRITE),
			"Could not open output file for \"%s\"", path
		);
	}
}

void SDR::Video::Writer::SetEncoder(AVCodec* encoder)
{
	Encoder = encoder;

	Stream = avformat_new_stream(FormatContext.Get(), Encoder);
	SDR::Error::ThrowIfNull(Stream, "Could not create video stream"s);

	/*
		Against what the new ffmpeg API incorrectly suggests, but is the right way.
	*/
	CodecContext = Stream->codec;
}

void SDR::Video::Writer::OpenEncoder(int framerate, AVDictionary** options)
{
	CodecContext->width = Frame->width;
	CodecContext->height = Frame->height;
	CodecContext->pix_fmt = (AVPixelFormat)Frame->format;
	CodecContext->colorspace = Frame->colorspace;
	CodecContext->color_range = Frame->color_range;

	if (FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		CodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	AVRational timebase;
	timebase.num = 1;
	timebase.den = framerate;

	auto inversetime = av_inv_q(timebase);

	CodecContext->time_base = timebase;
	CodecContext->framerate = inversetime;

	SDR::Error::LAV::ThrowIfFailed
	(
		avcodec_open2(CodecContext, Encoder, options),
		"Could not open video encoder"
	);

	SDR::Error::LAV::ThrowIfFailed
	(
		avcodec_parameters_from_context(Stream->codecpar, CodecContext),
		"Could not transfer encoder parameters to stream"
	);

	Stream->time_base = timebase;
	Stream->avg_frame_rate = inversetime;
}

void SDR::Video::Writer::WriteHeader()
{
	SDR::Error::LAV::ThrowIfFailed
	(
		avformat_write_header(FormatContext.Get(), nullptr),
		"Could not write container header"
	);
}

void SDR::Video::Writer::WriteTrailer()
{
	av_write_trailer(FormatContext.Get());
}

void SDR::Video::Writer::SetFrameInput(PlaneType& planes)
{
	int index = 0;

	for (auto& plane : planes)
	{
		if (plane.empty())
		{
			break;
		}

		Frame->data[index] = plane.data();
		++index;
	}
}

void SDR::Video::Writer::SendRawFrame()
{
	{
		Profile::ScopedEntry e1(Profile::Types::Encode);

		Frame->pts = PresentationIndex;
		PresentationIndex++;

		avcodec_send_frame(CodecContext, Frame.Get());
	}

	ReceivePacketFrame();
}

void SDR::Video::Writer::SendFlushFrame()
{
	avcodec_send_frame(CodecContext, nullptr);
	ReceivePacketFrame();
}

void SDR::Video::Writer::Finish()
{
	SendFlushFrame();
	WriteTrailer();
}

void SDR::Video::Writer::ReceivePacketFrame()
{
	int status = 0;

	AVPacket packet = {};
	av_init_packet(&packet);

	while (status == 0)
	{
		status = avcodec_receive_packet(CodecContext, &packet);

		if (status < 0)
		{
			return;
		}

		WriteEncodedPacket(packet);
	}
}

void SDR::Video::Writer::WriteEncodedPacket(AVPacket& packet)
{
	av_packet_rescale_ts(&packet, CodecContext->time_base, Stream->time_base);

	packet.stream_index = Stream->index;

	av_interleaved_write_frame(FormatContext.Get(), &packet);
}
