#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include "dbg.h"

#include "HLAE\HLAE.hpp"
#include "HLAE\Sampler.hpp"

extern "C"
{
	#include "libavutil\avutil.h"
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
	#include "libswscale\swscale.h"
}

#include <ppltasks.h>
#include "readerwriterqueue.h"

/*
	For WAVE related things
*/
#include <mmsystem.h>

namespace
{
	namespace LAV
	{
		enum class ExceptionType
		{
			AllocSWSContext,
			AllocCodecContext,
			AllocAVFrame,
			AllocVideoStream,
			VideoEncoderNotFound,
			OpenFile,
		};

		const char* ExceptionTypeToString(ExceptionType code)
		{
			auto index = static_cast<int>(code);

			static const char* names[] =
			{
				"Could not allocate video conversion context",
				"Could not allocate audio video codec context",
				"Could not allocate audio video frame",
				"Could not allocate video stream",
				"Video encoder not found",
				"Could not create file",
			};

			auto retstr = names[index];

			return retstr;
		}

		struct ExceptionNullPtr
		{
			ExceptionType Code;
			const char* Description;
		};

		struct Exception
		{
			int Code;
		};

		inline void ThrowIfNull(void* ptr, ExceptionType code)
		{
			if (!ptr)
			{
				auto desc = ExceptionTypeToString(code);

				Warning("SDR LAV: %s\n", desc);

				ExceptionNullPtr info;
				info.Code = code;
				info.Description = desc;

				throw info;
			}
		}

		inline void ThrowIfFailed(int code)
		{
			if (code < 0)
			{
				Warning("SDR LAV: %d\n", code);

				Exception info;
				info.Code = code;

				throw info;
			}
		}

		struct ScopedSWSContext
		{
			~ScopedSWSContext()
			{
				sws_freeContext(Context);
			}

			void Assign
			(
				int width,
				int height,
				AVPixelFormat sourceformat,
				AVPixelFormat destformat
			)
			{
				Context = sws_getContext
				(
					width,
					height,
					sourceformat,
					width,
					height,
					destformat,
					0,
					nullptr,
					nullptr,
					nullptr
				);

				ThrowIfNull(Context, LAV::ExceptionType::AllocSWSContext);
			}

			SwsContext* Get()
			{
				return Context;
			}

			SwsContext* Context = nullptr;
		};

		struct ScopedFormatContext
		{
			~ScopedFormatContext()
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

			void Assign(const char* filename)
			{
				ThrowIfFailed
				(
					avformat_alloc_output_context2(&Context, nullptr, nullptr, filename)
				);
			}

			AVFormatContext* Get() const
			{
				return Context;
			}

			auto operator->() const
			{
				return Get();
			}

			AVFormatContext* Context = nullptr;
		};

		struct ScopedCodecContext
		{
			~ScopedCodecContext()
			{
				if (Context)
				{
					avcodec_free_context(&Context);
				}
			}

			void Assign(AVCodec* codec)
			{
				Context = avcodec_alloc_context3(codec);

				ThrowIfNull(Context, LAV::ExceptionType::AllocCodecContext);
			}

			AVCodecContext* Get() const
			{
				return Context;
			}

			auto operator->() const
			{
				return Get();
			}

			explicit operator bool() const
			{
				return Get() != nullptr;
			}

			AVCodecContext* Context = nullptr;
		};

		struct ScopedAVFrame
		{
			~ScopedAVFrame()
			{
				if (Frame)
				{
					av_frame_free(&Frame);
				}
			}

			void Assign(AVPixelFormat format, int width, int height)
			{
				Frame = av_frame_alloc();

				ThrowIfNull(Frame, LAV::ExceptionType::AllocAVFrame);

				Frame->format = format;
				Frame->width = width;
				Frame->height = height;

				av_frame_get_buffer(Frame, 32);

				Frame->pts = 0;
			}

			AVFrame* Get() const
			{
				return Frame;
			}

			auto operator->() const
			{
				return Get();
			}

			AVFrame* Frame = nullptr;
		};

		struct ScopedAVDictionary
		{
			~ScopedAVDictionary()
			{
				av_dict_free(&Options);
			}

			AVDictionary** Get()
			{
				return &Options;
			}

			void Set(const char* key, const char* value, int flags = 0)
			{
				ThrowIfFailed
				(
					av_dict_set(Get(), key, value, flags)
				);
			}

			void ParseString(const char* string)
			{
				ThrowIfFailed
				(
					av_dict_parse_string(Get(), string, "=", " ", 0)
				);
			}

			AVDictionary* Options = nullptr;
		};

		struct ScopedFile
		{
			~ScopedFile()
			{
				Close();
			}

			void Close()
			{
				if (Handle)
				{
					fclose(Handle);
					Handle = nullptr;
				}
			}

			auto Get() const
			{
				return Handle;
			}

			explicit operator bool() const
			{
				return Get() != nullptr;
			}

			void Assign(const char* path, const char* mode)
			{
				Handle = fopen(path, mode);

				ThrowIfNull(Handle, ExceptionType::OpenFile);
			}

			template <typename... Types>
			void WriteSimple(const Types&... args)
			{
				bool adder[] =
				{
					[&]()
					{
						fwrite(&args, sizeof(args), 1, Get());
						return true;
					}()...
				};
			}

			auto GetStreamPosition() const
			{
				return ftell(Get());
			}

			void SeekAbsolute(int32_t position)
			{
				fseek(Get(), position, SEEK_SET);
			}

			FILE* Handle = nullptr;
		};
	}
}

namespace LAV
{
	namespace
	{
		namespace Variables
		{
			ConVar SuppressLog
			(
				"sdr_movie_suppresslog", "0", FCVAR_NEVER_AS_STRING,
				"Disable logging output from LAV",
				true, 0, true, 1
			);
		}
	}

	void LogFunction
	(
		void* avcl,
		int level,
		const char* fmt,
		va_list vl
	)
	{
		if (!Variables::SuppressLog.GetBool())
		{
			/*
				989 max limit according to
				https://developer.valvesoftware.com/wiki/Developer_Console_Control#Printing_to_the_console

				960 to keep in a 32 byte alignment
			*/
			char buf[960];
			vsprintf_s(buf, fmt, vl);

			/*
				Not formatting the buffer to a string will create
				a runtime error on any float conversion
			*/
			Msg("%s", buf);
		}
	}
}

namespace
{
	struct SDRAudioWriter
	{
		SDRAudioWriter()
		{
			SamplesToWrite.reserve(1024);
		}

		~SDRAudioWriter()
		{
			Finish();
		}

		void Open(const char* name, int samplerate, int samplebits, int channels)
		{
			WaveFile.Assign(name, "wb");

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

			WAVEFORMATEX waveformat = {0};
			std::memset(&waveformat, 0, sizeof(waveformat));

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

		void Finish()
		{
			/*
				Prevent reentry from destructor
			*/
			if (!WaveFile)
			{
				return;
			}

			for (auto& samples : SamplesToWrite)
			{
				WritePCM16Samples(samples);
			}

			WaveFile.SeekAbsolute(HeaderPosition);
			WaveFile.WriteSimple(FileLength - sizeof(int) * 2);

			WaveFile.SeekAbsolute(DataPosition);
			WaveFile.WriteSimple(DataLength);

			WaveFile.Close();
		}

		void AddPCM16Samples(std::vector<int16_t>&& samples)
		{
			SamplesToWrite.emplace_back(std::move(samples));
		}

		void WritePCM16Samples(const std::vector<int16_t>& samples)
		{
			auto buffer = samples.data();
			auto length = samples.size();

			fwrite(buffer, length, 1, WaveFile.Get());

			DataLength += length;
			FileLength += DataLength;
		}

		LAV::ScopedFile WaveFile;
		
		/*
			These variables are used to reference a stream position
			that needs data from the future
		*/
		int32_t HeaderPosition;
		int32_t DataPosition;
		
		int32_t DataLength = 0;
		int32_t FileLength = 0;

		std::vector<std::vector<int16_t>> SamplesToWrite;
	};

	struct SDRVideoWriter
	{
		void OpenFileForWrite(const char* path)
		{
			FormatContext.Assign(path);

			if (!(FormatContext->oformat->flags & AVFMT_NOFILE))
			{
				LAV::ThrowIfFailed
				(
					avio_open(&FormatContext->pb, path, AVIO_FLAG_WRITE)
				);
			}
		}

		void SetEncoder(const char* name)
		{
			Encoder = avcodec_find_encoder_by_name(name);

			LAV::ThrowIfNull(Encoder, LAV::ExceptionType::VideoEncoderNotFound);

			CodecContext.Assign(Encoder);

			if (FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
			{
				CodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}

			Stream = avformat_new_stream(FormatContext.Get(), Encoder);

			LAV::ThrowIfNull(Stream, LAV::ExceptionType::AllocVideoStream);
		}

		void SetCodecParametersToStream()
		{
			LAV::ThrowIfFailed
			(
				avcodec_parameters_from_context
				(
					Stream->codecpar,
					CodecContext.Get()
				)
			);
		}

		void OpenEncoder(AVDictionary** options)
		{
			LAV::ThrowIfFailed
			(
				avcodec_open2(CodecContext.Get(), Encoder, options)
			);

			SetCodecParametersToStream();
		}

		void WriteHeader()
		{
			LAV::ThrowIfFailed
			(
				avformat_write_header(FormatContext.Get(), nullptr)
			);
		}

		void WriteTrailer()
		{
			LAV::ThrowIfFailed
			(
				av_write_trailer(FormatContext.Get())
			);
		}

		void SetRGB24Input(uint8_t* buffer, int width, int height)
		{
			uint8_t* sourceplanes[] =
			{
				buffer
			};

			/*
				3 for 3 bytes per pixel
			*/
			int sourcestrides[] =
			{
				width * 3
			};

			if (CodecContext->pix_fmt == AV_PIX_FMT_RGB24 ||
				CodecContext->pix_fmt == AV_PIX_FMT_BGR24)
			{
				Frame->data[0] = sourceplanes[0];
				Frame->linesize[0] = sourcestrides[0];
			}

			else
			{
				sws_scale
				(
					FormatConverter.Get(),
					sourceplanes,
					sourcestrides,
					0,
					height,
					Frame->data,
					Frame->linesize
				);
			}
		}

		void SendRawFrame()
		{
			Frame->pts = PresentationIndex;

			auto ret = avcodec_send_frame
			(
				CodecContext.Get(),
				Frame.Get()
			);

			ReceivePacketFrame();

			PresentationIndex++;
		}

		void SendFlushFrame()
		{
			auto ret = avcodec_send_frame
			(
				CodecContext.Get(),
				nullptr
			);

			ReceivePacketFrame();
		}

		void Finish()
		{
			SendFlushFrame();
			WriteTrailer();
		}

		void ReceivePacketFrame()
		{
			int status = 0;

			AVPacket packet;
			av_init_packet(&packet);

			while (status == 0)
			{
				status = avcodec_receive_packet
				(
					CodecContext.Get(),
					&packet
				);

				if (status != 0)
				{
					return;
				}

				WriteEncodedPacket(packet);
			}
		}

		void WriteEncodedPacket(AVPacket& packet)
		{
			av_packet_rescale_ts
			(
				&packet,
				CodecContext->time_base,
				Stream->time_base
			);

			packet.stream_index = Stream->index;

			LAV::ThrowIfFailed
			(
				av_interleaved_write_frame
				(
					FormatContext.Get(),
					&packet
				)
			);

			av_packet_unref(&packet);
		}

		LAV::ScopedFormatContext FormatContext;
		
		LAV::ScopedCodecContext CodecContext;
		AVCodec* Encoder = nullptr;
		AVStream* Stream = nullptr;
		LAV::ScopedAVFrame Frame;

		/*
			Conversion from RGB24 to whatever specified
		*/
		LAV::ScopedSWSContext FormatConverter;

		/*
			Incremented and written to for every sent frame
		*/
		int64_t PresentationIndex = 0;
	};

	struct MovieData : public SDR::Sampler::IFramePrinter
	{
		struct VideoFutureSampleData
		{
			double Time;
			std::vector<uint8_t> Data;
		};

		using VideoQueueType = moodycamel::ReaderWriterQueue<VideoFutureSampleData>;

		enum
		{
			/*
				Order: Red Green Blue
			*/
			BytesPerPixel = 3
		};

		/*
			Not a threadsafe function as we only operate on
			a single AVFrame
		*/
		virtual void Print(uint8_t* data) override
		{
			/*
				The reason we override the default VID_ProcessMovieFrame is
				because that one creates a local CUtlBuffer for every frame and then destroys it.
				Better that we store them ourselves and iterate over the buffered frames to
				write them out in another thread.
			*/

			Video->SetRGB24Input(data, Width, Height);
			Video->SendRawFrame();
		}

		uint32_t GetRGB24ImageSize() const
		{
			return (Width * Height) * BytesPerPixel;
		}

		bool IsStarted = false;

		uint32_t Width;
		uint32_t Height;

		double SamplingTime = 0.0;

		std::unique_ptr<SDRVideoWriter> Video;
		std::unique_ptr<SDRAudioWriter> Audio;

		std::unique_ptr<SDR::Sampler::EasyByteSampler> Sampler;

		int32_t BufferedFrames = 0;
		std::unique_ptr<VideoQueueType> FramesToSampleBuffer;
		std::thread FrameHandlerThread;
	};

	MovieData CurrentMovie;
	std::atomic_bool ShouldStopFrameThread = false;

	void SDR_MovieShutdown()
	{
		if (!CurrentMovie.IsStarted)
		{
			return;
		}

		if (!ShouldStopFrameThread)
		{
			ShouldStopFrameThread = true;
			CurrentMovie.FrameHandlerThread.join();
		}

		CurrentMovie = MovieData();
	}

	/*
		In case endmovie never gets called,
		this handles the plugin_unload
	*/
	SDR::PluginShutdownFunctionAdder A1(SDR_MovieShutdown);
}

namespace
{
	namespace Variables
	{
		ConVar FrameBufferSize
		(
			"sdr_frame_buffersize", "256", FCVAR_NEVER_AS_STRING,
			"How many frames that are allowed to be buffered up for encoding. "
			"This value can be lowered or increased depending your available RAM. "
			"Keep in mind the sizes of an uncompressed RGB24 frame: \n"
			"1280x720    : 2.7 MB\n"
			"1920x1080   : 5.9 MB\n"
			"Calculation : (((x * y) * 3) / 1024) / 1024"
			"\n"
			"Multiply the frame size with the buffer size to one that fits you.\n"
			"*** Using too high of a buffer size might eventually crash the application "
			"if there no longer is any available memory ***\n"
			"\n"
			"The frame buffer queue will only build up and fall behind when the encoding "
			"is taking too long, consider not using too low of a preset.",
			true, 8, true, 384
		);

		ConVar FrameRate
		(
			"sdr_render_framerate", "60", FCVAR_NEVER_AS_STRING,
			"Movie output framerate",
			true, 30, true, 1000
		);

		ConVar Exposure
		(
			"sdr_render_exposure", "0.5", FCVAR_NEVER_AS_STRING,
			"Frame exposure fraction",
			true, 0, true, 1
		);

		ConVar SamplesPerSecond
		(
			"sdr_render_samplespersecond", "600", FCVAR_NEVER_AS_STRING,
			"Game framerate in samples",
			true, 0, false, 0
		);

		ConVar FrameStrength
		(
			"sdr_render_framestrength", "1.0", FCVAR_NEVER_AS_STRING,
			"Controls clearing of the sampling buffer upon framing. "
			"The lower the value the more cross-frame motion blur",
			true, 0, true, 1
		);

		ConVar SampleMethod
		(
			"sdr_render_samplemethod", "1", FCVAR_NEVER_AS_STRING,
			"Selects the integral approximation method: "
			"0: 1 point, rectangle method, 1: 2 point, trapezoidal rule",
			true, 0, true, 1
		);

		ConVar OutputDirectory
		(
			"sdr_outputdir", "", 0,
			"Where to save the output frames."
		);

		ConVar FlashWindow
		(
			"sdr_endmovieflash", "0", FCVAR_NEVER_AS_STRING,
			"Flash window when endmovie is called",
			true, 0, true, 1
		);

		ConVar ExitOnFinish
		(
			"sdr_endmoviequit", "0", FCVAR_NEVER_AS_STRING,
			"Quit game when endmovie is called",
			true, 0, true, 1
		);

		namespace Audio
		{
			ConVar Enable
			(
				"sdr_audio_enable", "0", FCVAR_NEVER_AS_STRING,
				"Process audio as well",
				true, 0, true, 1
			);
		}

		namespace Video
		{
			ConVar PixelFormat
			(
				"sdr_movie_encoder_pxformat", "", 0,
				"Video pixel format"
				"Values: Depends on encoder, view Github page"
			);

			namespace X264
			{
				ConVar CRF
				(
					"sdr_x264_crf", "10", 0,
					"Constant rate factor value. Values: 0 (best) - 51 (worst). "
					"See https://trac.ffmpeg.org/wiki/Encode/H.264"
				);

				ConVar Preset
				{
					"sdr_x264_preset", "medium", 0,
					"X264 encoder preset. See https://trac.ffmpeg.org/wiki/Encode/H.264\n"
					"Important note: Optimally, do not use a too low of a preset as the streaming "
					"needs to be somewhat realtime.",
					[](IConVar* var, const char* oldstr, float oldfloat)
					{
						auto newstr = Preset.GetString();

						auto slowpresets =
						{
							"slow",
							"slower",
							"veryslow",
							"placebo"
						};

						for (auto preset : slowpresets)
						{
							if (_strcmpi(newstr, preset) == 0)
							{
								Warning
								(
									"SDR: Slow encoder preset chosen, "
									"this might not work very well for realtime\n"
								);

								return;
							}
						}
					}
				};

				ConVar Tune
				(
					"sdr_x264_tune", "", 0,
					"X264 encoder tune. See https://trac.ffmpeg.org/wiki/Encode/H.264"
				);
			}

			ConVar ColorSpace
			(
				"sdr_movie_encoder_colorspace", "601", 0,
				"Possible values: 601, 709"
			);

			ConVar ColorRange
			(
				"sdr_movie_encoder_colorrange", "partial", 0,
				"Possible values: full, partial"
			);
		}
	}

	void FrameThreadHandler()
	{
		auto& interfaces = SDR::GetEngineInterfaces();
		auto& movie = CurrentMovie;
		auto& nonreadyframes = movie.FramesToSampleBuffer;

		auto spsvar = static_cast<double>(Variables::SamplesPerSecond.GetInt());
		auto sampleframerate = 1.0 / spsvar;

		MovieData::VideoFutureSampleData videosample;
		videosample.Data.reserve(movie.GetRGB24ImageSize());

		while (!ShouldStopFrameThread)
		{
			while (nonreadyframes->try_dequeue(videosample))
			{
				movie.BufferedFrames--;

				auto time = videosample.Time;

				if (movie.Sampler->CanSkipConstant(time, sampleframerate))
				{
					movie.Sampler->Sample(nullptr, time);
				}

				else
				{
					auto data = videosample.Data.data();
					movie.Sampler->Sample(data, time);
				}
			}
		}
	}

	namespace Module_StartMovie
	{
		/*
			0x100BCAC0 static CSS IDA address May 22 2016
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00"
			"\x00\x00\xD9\x45\x18\x56\x57\xF3\x0F\x10\x40\x00"
		);

		auto Mask =
		(
			"xxxxx????x????xxxxxxxxx?"
		);

		void __cdecl Override
		(
			const char* filename, int flags, int width, int height,
			float framerate, int jpegquality, int unk
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "StartMovie", Override, Pattern, Mask
		};

		/*
			The 7th parameter (unk) was been added in Source 2013,
			it's not there in Source 2007
		*/
		void __cdecl Override
		(
			const char* filename, int flags, int width, int height,
			float framerate, int jpegquality, int unk
		)
		{
			auto sdrpath = Variables::OutputDirectory.GetString();

			auto res = SHCreateDirectoryExA(nullptr, sdrpath, nullptr);

			switch (res)
			{
				case ERROR_SUCCESS:
				case ERROR_ALREADY_EXISTS:
				case ERROR_FILE_EXISTS:
				{
					break;
				}

				case ERROR_BAD_PATHNAME:
				case ERROR_PATH_NOT_FOUND:
				case ERROR_FILENAME_EXCED_RANGE:
				{
					Warning("SDR: Movie output path is invalid\n");
					return;
				}

				case ERROR_CANCELLED:
				{
					Warning("SDR: Extra directories were created but are hidden, aborting\n");
					return;
				}

				default:
				{
					Warning
					(
						"SDR: Some unknown error happened when starting movie, "
						"related to sdr_outputdir\n"
					);
					return;
				}
			}

			struct VideoConfigurationData
			{
				const char* EncoderName;
				AVCodecID EncoderType;

				std::vector<std::pair<const char*, AVPixelFormat>> PixelFormats;

				bool ImageSequence = false;
				const char* SequenceExtension;
			};

			auto seqremovepercent = [](char* filename)
			{
				auto length = strlen(filename);

				for (int i = length - 1; i >= 0; i--)
				{
					auto& curchar = filename[i];

					if (curchar == '%')
					{
						curchar = 0;
						break;
					}
				}
			};

			std::vector<VideoConfigurationData> videoconfigs;

			{
				auto i420 = std::make_pair("i420", AV_PIX_FMT_YUV420P);
				auto i444 = std::make_pair("i444", AV_PIX_FMT_YUV444P);
				auto nv12 = std::make_pair("nv12", AV_PIX_FMT_NV12);
				auto rgb24 = std::make_pair("rgb24", AV_PIX_FMT_RGB24);
				auto bgr24 = std::make_pair("bgr24", AV_PIX_FMT_BGR24);

				videoconfigs.emplace_back();
				auto& x264 = videoconfigs.back();				
				x264.EncoderName = "libx264";
				x264.EncoderType = AV_CODEC_ID_H264;
				x264.PixelFormats =
				{
					i420,
					i444,
					nv12,
				};

				videoconfigs.emplace_back();
				auto& huffyuv = videoconfigs.back();
				huffyuv.EncoderName = "huffyuv";
				huffyuv.EncoderType = AV_CODEC_ID_HUFFYUV;
				huffyuv.PixelFormats =
				{
					rgb24,
				};

				videoconfigs.emplace_back();
				auto& png = videoconfigs.back();
				png.EncoderName = "png";
				png.EncoderType = AV_CODEC_ID_PNG;
				png.ImageSequence = true;
				png.SequenceExtension = "png";
				png.PixelFormats =
				{
					rgb24,
				};

				videoconfigs.emplace_back();
				auto& targa = videoconfigs.back();
				targa.EncoderName = "targa";
				targa.EncoderType = AV_CODEC_ID_TARGA;
				targa.ImageSequence = true;
				targa.SequenceExtension = "tga";
				targa.PixelFormats =
				{
					bgr24,
				};
			}

			const VideoConfigurationData* vidconfig = nullptr;

			auto& movie = CurrentMovie;
				
			movie.Width = width;
			movie.Height = height;

			{
				try
				{
					movie.Video = std::make_unique<SDRVideoWriter>();

					if (Variables::Audio::Enable.GetBool())
					{
						movie.Audio = std::make_unique<SDRAudioWriter>();
					}

					auto vidwriter = movie.Video.get();
					auto audiowriter = movie.Audio.get();

					{						
						char finalfilename[1024];
						strcpy_s(finalfilename, filename);

						std::string extension;
						{
							auto ptr = V_GetFileExtension(finalfilename);

							if (ptr)
							{
								extension = ptr;
							}
						}

						/*
							Default to avi container with x264
						*/
						if (extension.empty())
						{
							extension = "libx264";
							strcat_s(finalfilename, ".avi.libx264");
						}

						/*
							Users can select what available encoder they want
						*/
						for (const auto& config : videoconfigs)
						{
							auto tester = config.EncoderName;

							if (config.ImageSequence)
							{
								tester = config.SequenceExtension;
							}

							if (_strcmpi(extension.c_str(), tester) == 0)
							{
								vidconfig = &config;
								break;
							}
						}

						/*
							None selected by user, use x264
						*/
						if (!vidconfig)
						{
							vidconfig = &videoconfigs[0];
						}

						else
						{
							V_StripExtension(finalfilename, finalfilename, sizeof(finalfilename));

							if (!vidconfig->ImageSequence)
							{
								auto ptr = V_GetFileExtension(finalfilename);

								if (!ptr)
								{
									ptr = "avi";
									strcat_s(finalfilename, ".avi");
								}
							}
						}

						if (vidconfig->ImageSequence)
						{
							seqremovepercent(finalfilename);
							strcat_s(finalfilename, "%05d.");
							strcat_s(finalfilename, vidconfig->SequenceExtension);
						}

						char finalname[2048];

						V_ComposeFileName
						(
							sdrpath,
							finalfilename,
							finalname,
							sizeof(finalname)
						);

						vidwriter->OpenFileForWrite(finalname);

						if (audiowriter)
						{
							V_StripExtension(finalname, finalname, sizeof(finalname));
							
							/*
								If the user wants an image sequence, it means the filename
								has the digit formatting, don't want this in audio name
							*/
							if (vidconfig->ImageSequence)
							{
								seqremovepercent(finalname);
							}

							strcat_s(finalname, ".wav");

							/*
								This is the only supported audio output format
							*/
							audiowriter->Open(finalname, 44'100, 16, 2);
						}

						auto formatcontext = vidwriter->FormatContext.Get();
						auto oformat = formatcontext->oformat;

						vidwriter->SetEncoder(vidconfig->EncoderName);
					}

					auto linktabletovariable = []
					(
						const char* key,
						const auto& table,
						auto& variable
					)
					{
						for (const auto& entry : table)
						{
							if (_strcmpi(key, entry.first) == 0)
							{
								variable = entry.second;
								break;
							}
						}
					};

					AVRational timebase;
					timebase.num = 1;
					timebase.den = Variables::FrameRate.GetInt();

					vidwriter->Stream->time_base = timebase;

					auto codeccontext = vidwriter->CodecContext.Get();
					codeccontext->codec_type = AVMEDIA_TYPE_VIDEO;
					codeccontext->width = width;
					codeccontext->height = height;
					codeccontext->time_base = timebase;

					auto pxformat = AV_PIX_FMT_NONE;

					auto pxformatstr = Variables::Video::PixelFormat.GetString();

					linktabletovariable(pxformatstr, vidconfig->PixelFormats, pxformat);

					if (pxformat == AV_PIX_FMT_NONE)
					{
						Msg("SDR: No compatible pixel format selected, using config default\n");
						pxformat = vidconfig->PixelFormats[0].second;
					}

					codeccontext->codec_id = vidconfig->EncoderType;

					if (pxformat != AV_PIX_FMT_RGB24 && pxformat != AV_PIX_FMT_BGR24)
					{
						vidwriter->FormatConverter.Assign
						(
							width,
							height,
							AV_PIX_FMT_RGB24,
							pxformat
						);
					}

					codeccontext->pix_fmt = pxformat;
		
					/*
						Not setting this will leave different colors across
						multiple programs
					*/

					if (pxformat == AV_PIX_FMT_RGB24 || pxformat == AV_PIX_FMT_BGR24)
					{
						codeccontext->color_range = AVCOL_RANGE_UNSPECIFIED;
						codeccontext->colorspace = AVCOL_SPC_RGB;
					}

					else
					{
						{
							auto space = Variables::Video::ColorSpace.GetString();

							auto table =
							{
								std::make_pair("601", AVCOL_SPC_BT470BG),
								std::make_pair("709", AVCOL_SPC_BT709)
							};

							linktabletovariable(space, table, codeccontext->colorspace);
						}

						{
							auto range = Variables::Video::ColorRange.GetString();

							auto table =
							{
								std::make_pair("full", AVCOL_RANGE_JPEG),
								std::make_pair("partial", AVCOL_RANGE_MPEG)
							};

							linktabletovariable(range, table, codeccontext->color_range);
						}
					}

					{
						if (vidconfig->EncoderType == AV_CODEC_ID_H264)
						{
							auto preset = Variables::Video::X264::Preset.GetString();
							auto tune = Variables::Video::X264::Tune.GetString();
							auto crf = Variables::Video::X264::CRF.GetString();

							LAV::ScopedAVDictionary options;
							options.Set("preset", preset);

							if (strlen(tune) > 0)
							{
								options.Set("tune", tune);
							}

							options.Set("crf", crf);

							/*
								Setting every frame as a keyframe
								gives the ability to use the video in a video editor with ease
							*/
							options.Set("x264-params", "keyint=1");

							vidwriter->OpenEncoder(options.Get());
						}

						else
						{
							vidwriter->OpenEncoder(nullptr);
						}
					}

					vidwriter->Frame.Assign
					(
						pxformat,
						width,
						height
					);

					vidwriter->WriteHeader();
				}
					
				catch (const LAV::Exception& error)
				{
					return;
				}

				catch (const LAV::ExceptionNullPtr& error)
				{
					return;
				}
			}

			ThisHook.GetOriginal()(filename, flags, width, height, framerate, jpegquality, unk);

			/*
				The original function sets host_framerate to 30 so we override it
			*/
			ConVarRef hostframerate("host_framerate");
			hostframerate.SetValue(Variables::SamplesPerSecond.GetInt());
				
			using SampleMethod = SDR::Sampler::EasySamplerSettings::Method;
			SampleMethod moviemethod = SampleMethod::ESM_Trapezoid;
			
			{
				auto table =
				{
					SampleMethod::ESM_Rectangle,
					SampleMethod::ESM_Trapezoid
				};

				auto key = Variables::SampleMethod.GetInt();
				
				for (const auto& entry : table)
				{
					if (key == entry)
					{
						moviemethod = entry;
						break;
					}
				}
			}

			auto framepitch = HLAE::CalcPitch(width, MovieData::BytesPerPixel, 1);
			auto frameratems = 1.0 / static_cast<double>(Variables::FrameRate.GetInt());
			auto exposure = Variables::Exposure.GetFloat();
			auto framestrength = Variables::FrameStrength.GetFloat();
			auto stride = MovieData::BytesPerPixel * width;

			SDR::Sampler::EasySamplerSettings settings
			(
				stride,
				height,
				moviemethod,
				frameratems,
				0.0,
				exposure,
				framestrength
			);

			auto size = movie.GetRGB24ImageSize();

			movie.IsStarted = true;

			movie.Sampler = std::make_unique<SDR::Sampler::EasyByteSampler>
			(
				settings,
				framepitch,
				&movie
			);

			movie.FramesToSampleBuffer = std::make_unique<MovieData::VideoQueueType>(128);

			ShouldStopFrameThread = false;
			movie.FrameHandlerThread = std::thread(FrameThreadHandler);
		}
	}

	namespace Module_StartMovieCommand
	{
		/*
			0x100BEF40 static CSS IDA address April 7 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x83\xEC\x08\x83\x3D\x00\x00\x00\x00"
			"\x00\x0F\x85\x00\x00\x00\x00\x8B\x4D\x08\x56\x8B"
			"\x01\x83\xF8\x02\x7D\x6B\x8B\x35\x00\x00\x00\x00"
		);

		auto Mask =
		(
			"xxxxxxxx?????xx????xxxxxxxxxxxxx????"
		);

		void __cdecl Override(const CCommand& args);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "StartMovieCommand", Override, Pattern, Mask
		};

		/*
			This command is overriden to remove the incorrect description
		*/
		void __cdecl Override(const CCommand& args)
		{
			if (args.ArgC() < 2)
			{
				ConMsg("SDR: Name is required for startmovie, see Github page for help\n");
				return;
			}

			int width;
			int height;
			g_pMaterialSystem->GetBackBufferDimensions(width, height);

			auto name = args[1];

			/*
				4 = FMOVIE_WAV, needed for future audio calls
			*/
			Module_StartMovie::Override(name, 4, width, height, 0, 0, 0);
		}
	}

	namespace Module_EndMovie
	{
		/*
			0x100BAE40 static CSS IDA address May 22 2016
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x80\x3D\x00\x00\x00\x00\x00\x0F\x84\x00\x00\x00\x00"
			"\xD9\x05\x00\x00\x00\x00\x51\xB9\x00\x00\x00\x00"
		);

		auto Mask =
		(
			"xx?????xx????xx????xx????"
		);

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "EndMovie", Override, Pattern, Mask
		};

		void __cdecl Override()
		{
			ThisHook.GetOriginal()();

			if (!CurrentMovie.IsStarted)
			{
				return;
			}

			ConVarRef hostframerate("host_framerate");
			hostframerate.SetValue(0);

			Msg("SDR: Ending movie, if there are buffered frames this might take a moment\n");

			auto task = concurrency::create_task([]()
			{
				/*
					Let the worker thread complete the sampling and
					giving the frames to the encoder
				*/
				if (!ShouldStopFrameThread)
				{
					ShouldStopFrameThread = true;
					CurrentMovie.FrameHandlerThread.join();
				}

				if (CurrentMovie.Audio)
				{
					CurrentMovie.Audio->Finish();
				}

				/*
					Let the encoder finish all the delayed frames
				*/
				CurrentMovie.Video->Finish();
			});

			task.then([]()
			{
				SDR_MovieShutdown();

				if (Variables::ExitOnFinish.GetBool())
				{
					auto& interfaces = SDR::GetEngineInterfaces();
					interfaces.EngineClient->ClientCmd("quit\n");
					
					return;
				}

				if (Variables::FlashWindow.GetBool())
				{
					auto& interfaces = SDR::GetEngineInterfaces();
					interfaces.EngineClient->FlashWindow();
				}

				ConColorMsg
				(
					Color(88, 255, 39, 255),
					"SDR: Movie is now complete\n"
				);
			});
		}
	}

	namespace Module_WriteMovieFrame
	{
		/*
			0x102011B0 static CSS IDA address June 3 2016
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x51\x80\x3D\x00\x00\x00\x00\x00\x53"
			"\x8B\x5D\x08\x57\x8B\xF9\x8B\x83\x00\x00\x00\x00"
		);

		auto Mask =
		(
			"xxxxxx?????xxxxxxxxx????"
		);

		/*
			The "thisptr" in this context is a CVideoMode_MaterialSystem in this structure:
			
			CVideoMode_MaterialSystem
				CVideoMode_Common
					IVideoMode

			WriteMovieFrame belongs to CVideoMode_Common and ReadScreenPixels overriden
			by CVideoMode_MaterialSystem.
			The global engine variable "videomode" is of type CVideoMode_MaterialSystem
			which is what called WriteMovieFrame.
			
			For more usage see: VideoMode_Create (0x10201130) and VideoMode_Destroy (0x10201190)
			Static CSS IDA addresses June 3 2016

			The purpose of overriding this function completely is to prevent the constant image buffer
			allocation that Valve does every movie frame. We just provide one buffer that gets reused.
		*/
		void __fastcall Override
		(
			void* thisptr, void* edx, void* info
		)
		{
			auto width = CurrentMovie.Width;
			auto height = CurrentMovie.Height;

			auto& movie = CurrentMovie;

			auto spsvar = static_cast<double>(Variables::SamplesPerSecond.GetInt());
			auto sampleframerate = 1.0 / spsvar;

			auto& time = movie.SamplingTime;

			MovieData::VideoFutureSampleData newsample;
			newsample.Time = time;
			newsample.Data.resize(movie.GetRGB24ImageSize());

			auto pxformat = IMAGE_FORMAT_RGB888;

			if (movie.Video->CodecContext->pix_fmt == AV_PIX_FMT_BGR24)
			{
				pxformat = IMAGE_FORMAT_BGR888;
			}

			auto rendercontext = g_pMaterialSystem->GetRenderContext();
			rendercontext->ReadPixels(0, 0, width, height, newsample.Data.data(), pxformat);

			auto buffersize = Variables::FrameBufferSize.GetInt();

			/*
				Encoder is falling behind, too much input with too little output
			*/
			if (movie.BufferedFrames > buffersize)
			{
				Warning("SDR: Too many buffered frames, waiting for encoder\n");

				while (movie.BufferedFrames > 1)
				{
					std::this_thread::sleep_for(1ms);
				}

				Warning
				(
					"SDR: Encoder caught up, consider using faster encoding settings or "
					"increasing sdr_frame_buffersize.\n"
					R"(Type "help sdr_frame_buffersize" for more information.)" "\n"
				);
			}

			movie.BufferedFrames++;
			movie.FramesToSampleBuffer->enqueue(std::move(newsample));

			time += sampleframerate;
		}

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "WriteMovieFrame", Override, Pattern, Mask
		};
	}

	namespace Module_SNDRecordBuffer
	{
		/*
			0x1007C710 static CSS IDA address March 21 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x8B\x0D\x00\x00\x00\x00\x53\x56\x57\x85"
			"\xC9\x74\x0B\x8B\x01\x8B\x40\x38\xFF\xD0\x84\xC0\x75"
			"\x0D\x80\x3D\x00\x00\x00\x00\x00\x0F\x84\x00\x00\x00"
			"\x00"
		);
		
		auto Mask =
		(
			"xxxxx????xxxxxxxxxxxxxxxxxxxx?????xx????"
		);

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "SNDRecordBuffer", Override, Pattern, Mask
		};

		void __cdecl Override()
		{
			if (CurrentMovie.Audio)
			{
				ThisHook.GetOriginal()();
			}
		}
	}

	namespace Module_WaveCreateTmpFile
	{
		/*
			0x1008EC90 static CSS IDA address March 21 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\x8D\x85\x00\x00"
			"\x00\x00\x57\x68\x00\x00\x00\x00\x50\xFF\x75\x08\xE8"
			"\x00\x00\x00\x00\x68\x00\x00\x00\x00\x8D\x85\x00\x00"
			"\x00\x00\x68\x00\x00\x00\x00\x50\xE8\x00\x00\x00\x00"
			"\x8B\x0D\x00\x00\x00\x00\x8D\x95\x00\x00\x00\x00\x83"
			"\xC4\x18\x83\xC1\x04\x8B\x01\x6A\x00\x68\x00\x00\x00"
			"\x00\x52\xFF\x50\x08\x8B\xF8\x85\xFF\x0F\x84\x00\x00"
			"\x00\x00"
		);
		
		auto Mask =
		(
			"xxxxx????xx????xx????xxxxx????x????xx????x????xx????"
			"xx????xx????xxxxxxxxxxx????xxxxxxxxxx????"
		);

		void __cdecl Override
		(
			const char* filename, int rate, int bits, int channels
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "WaveCreateTmpFile", Override, Pattern, Mask
		};

		/*
			"rate" will always be 44100
			"bits" will always be 16
			"channels" will always be 2

			According to engine\audio\private\snd_mix.cpp @ 4003
		*/
		void __cdecl Override
		(
			const char* filename, int rate, int bits, int channels
		)
		{
			
		}
	}

	namespace Module_WaveAppendTmpFile
	{
		/*
			0x1008EBE0 static CSS IDA address March 21 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\x8D\x85\x00\x00"
			"\x00\x00\x57\x68\x00\x00\x00\x00\x50\xFF\x75\x08\xE8"
			"\x00\x00\x00\x00\x68\x00\x00\x00\x00\x8D\x85\x00\x00"
			"\x00\x00\x68\x00\x00\x00\x00\x50\xE8\x00\x00\x00\x00"
			"\x8B\x0D\x00\x00\x00\x00\x8D\x95\x00\x00\x00\x00\x83"
			"\xC4\x18\x83\xC1\x04\x8B\x01\x6A\x00\x68\x00\x00\x00"
			"\x00\x52\xFF\x50\x08\x8B\xF8\x85\xFF\x74\x47"
		);

		auto Mask =
		(
			"xxxxx????xx????xx????xxxxx????x????xx????x????xx????"
			"xx????xx????xxxxxxxxxxx????xxxxxxxxxx"
		);

		void __cdecl Override
		(
			const char* filename, void* buffer, int samplebits, int samplecount
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "WaveAppendTmpFile", Override, Pattern, Mask
		};

		/*
			"samplebits" will always be 16

			According to engine\audio\private\snd_mix.cpp @ 4074
		*/
		void __cdecl Override
		(
			const char* filename, void* buffer, int samplebits, int samplecount
		)
		{
			auto bufstart = static_cast<int16_t*>(buffer);
			auto length = samplecount * samplebits / 8;

			CurrentMovie.Audio->AddPCM16Samples({bufstart, bufstart + length});
		}
	}

	namespace Module_WaveFixupTmpFile
	{
		/*
			0x1008EE30 static CSS IDA address March 21 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\x8D\x85\x00\x00"
			"\x00\x00\x56\x68\x00\x00\x00\x00\x50\xFF\x75\x08\xE8"
			"\x00\x00\x00\x00\x68\x00\x00\x00\x00\x8D\x85\x00\x00"
			"\x00\x00\x68\x00\x00\x00\x00\x50\xE8\x00\x00\x00\x00"
			"\x8B\x0D\x00\x00\x00\x00\x8D\x95\x00\x00\x00\x00\x83"
			"\xC4\x18\x83\xC1\x04\x8B\x01\x6A\x00\x68\x00\x00\x00"
			"\x00"
		);
		
		auto Mask =
		(
			"xxxxx????xx????xx????xxxxx????x????xx????x????xx????x"
			"x????xx????xxxxxxxxxxx????"
		);

		void __cdecl Override(const char* filename);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "WaveFixupTmpFile", Override, Pattern, Mask
		};

		/*
			Gets called when the movie is ended
		*/
		void __cdecl Override(const char* filename)
		{
			
		}
	}
}
