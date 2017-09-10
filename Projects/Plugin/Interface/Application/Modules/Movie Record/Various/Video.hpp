#pragma once
#include <array>
#include <vector>
#include <cstdint>

#include "LAV.hpp"
#include "Pool.hpp"

namespace SDR::Video
{
	struct Writer
	{
		/*
			At most, YUV formats will use all planes. RGB only uses 1.
		*/
		using PlaneType = std::array<void*, 3>;

		void OpenFileForWrite(const char* path);
		void SetEncoder(AVCodec* encoder);
		void OpenEncoder(int framerate, AVDictionary** options);

		void WriteHeader();
		void WriteTrailer();

		void SetFrameInput(const PlaneType& planes);
		void SendRawFrame();
		void SendFlushFrame();

		void Finish();
		void ReceivePacketFrame();
		void WriteEncodedPacket(AVPacket& packet);

		SDR::LAV::ScopedFormatContext FormatContext;

		/*
			This gets freed when FormatContext gets destroyed.
		*/
		AVCodecContext* CodecContext;
		AVCodec* Encoder = nullptr;
		AVStream* Stream = nullptr;
		SDR::LAV::ScopedAVFrame Frame;

		/*
			Incremented for every sent frame.
		*/
		int64_t PresentationIndex = 0;
	};
}
