#pragma once
#include "Video.hpp"

namespace SDR::Stream
{
	/*
		This structure is sent to the encoder thread from the capture thread.
	*/
	struct FutureData
	{
		SDR::Video::Writer* Writer;
		SDR::Video::Writer::PlaneType Planes;
	};
}
