#pragma once
#include <SDR Shared\File.hpp>
#include <SDR Shared\Error.hpp>
#include <mmsystem.h>

namespace SDR::Audio
{
	struct Writer
	{
		~Writer();

		void Open(const wchar_t* name, int samplerate, int samplebits, int channels);

		void Finish();

		void WritePCM16Samples(const std::vector<int16_t>& samples);

		SDR::File::ScopedFile WaveFile;
		
		/*
			These variables are used to reference a stream position
			that needs data from the future
		*/
		int32_t HeaderPosition;
		int32_t DataPosition;
		
		int32_t DataLength = 0;
		int32_t FileLength = 0;
	};
}
