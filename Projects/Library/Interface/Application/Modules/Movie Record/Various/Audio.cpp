#include "PrecompiledHeader.hpp"
#include "Audio.hpp"

SDR::Audio::Writer::~Writer()
{
	Finish();
}

void SDR::Audio::Writer::Open(const wchar_t* name, int samplerate, int samplebits, int channels)
{
	try
	{
		WaveFile.Assign(name, L"wb");
	}

	catch (SDR::File::ScopedFile::ExceptionType status)
	{
		SDR::Error::Make("Could not create audio file");
	}

	enum : int32_t
	{
		RIFF = MAKEFOURCC('R', 'I', 'F', 'F'),
		WAVE = MAKEFOURCC('W', 'A', 'V', 'E'),
		FMT_ = MAKEFOURCC('f', 'm', 't', ' '),
		DATA = MAKEFOURCC('d', 'a', 't', 'a')
	};

	WaveFile.WriteSimple(RIFF, 0);

	HeaderPosition = WaveFile.GetStreamPosition() - sizeof(int);

	WaveFile.WriteSimple(WAVE);

	WAVEFORMATEX waveformat = {};
	waveformat.wFormatTag = WAVE_FORMAT_PCM;
	waveformat.nChannels = channels;
	waveformat.nSamplesPerSec = samplerate;
	waveformat.nAvgBytesPerSec = samplerate * samplebits * channels / 8;
	waveformat.nBlockAlign = (channels * samplebits) / 8;
	waveformat.wBitsPerSample = samplebits;

	WaveFile.WriteSimple(FMT_, sizeof(waveformat), waveformat);
	WaveFile.WriteSimple(DATA, 0);

	FileLength = WaveFile.GetStreamPosition();
	DataPosition = FileLength - sizeof(int);
}

void SDR::Audio::Writer::Finish()
{
	/*
		Prevent reentry from destructor
	*/
	if (!WaveFile)
	{
		return;
	}

	WaveFile.SeekAbsolute(HeaderPosition);
	WaveFile.WriteSimple(FileLength - sizeof(int) * 2);

	WaveFile.SeekAbsolute(DataPosition);
	WaveFile.WriteSimple(DataLength);

	WaveFile.Close();
}

void SDR::Audio::Writer::WritePCM16Samples(const std::vector<int16_t>& samples)
{
	auto buffer = samples.data();
	auto length = samples.size();

	WaveFile.WriteRegion(buffer, length);

	DataLength += length;
	FileLength += DataLength;
}
