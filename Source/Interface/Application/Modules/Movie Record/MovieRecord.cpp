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

#include "readerwriterqueue.h"

namespace LAV
{
	void LogFunction
	(
		void* avcl,
		int level,
		const char* fmt,
		va_list vl
	)
	{
		MsgV(fmt, vl);
	}
}

namespace
{
	namespace LAV
	{
		enum class ExceptionType
		{
			AllocSWSContext,
			AllocCodecContext,
			AllocAVFrame,
			AllocAVStream,
			EncoderNotFound,
		};

		const char* ExceptionTypeToString(ExceptionType code)
		{
			auto index = static_cast<int>(code);

			static const char* names[] =
			{
				"Could not allocate pixel format conversion context",
				"Could not allocate codec context",
				"Could not allocate video frame",
				"Could not allocate video stream",
				"Encoder not found"
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

		struct ScopedAVFContext
		{
			~ScopedAVFContext()
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

			AVCodecContext* Context = nullptr;
		};

		struct ScopedAVCFrame
		{
			~ScopedAVCFrame()
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

			AVFrame* Frame = nullptr;
		};
	}
}

namespace
{
	struct SDRVideoWriter
	{
		void OpenFileForWrite(const char* path)
		{
			FormatContext.Assign(path);

			if (!(FormatContext.Context->oformat->flags & AVFMT_NOFILE))
			{
				LAV::ThrowIfFailed
				(
					avio_open(&FormatContext.Context->pb, path, AVIO_FLAG_WRITE)
				);
			}
		}

		void SetEncoder(const char* name)
		{
			Encoder = avcodec_find_encoder_by_name(name);

			LAV::ThrowIfNull(Encoder, LAV::ExceptionType::EncoderNotFound);

			CodecContext.Assign(Encoder);
		}

		AVStream* AddStream()
		{
			auto retptr = avformat_new_stream(FormatContext.Get(), Encoder);

			LAV::ThrowIfNull(retptr, LAV::ExceptionType::AllocAVStream);

			return retptr;
		}

		void SetCodecParametersToStream()
		{
			LAV::ThrowIfFailed
			(
				avcodec_parameters_from_context
				(
					VideoStream->codecpar,
					CodecContext.Get()
				)
			);
		}

		void OpenEncoder(AVDictionary** options)
		{
			SetCodecParametersToStream();

			LAV::ThrowIfFailed
			(
				avcodec_open2(CodecContext.Get(), Encoder, options)
			);
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

			int sourcestrides[] =
			{
				width * 3
			};

			if (CodecContext.Context->pix_fmt == AV_PIX_FMT_RGB24)
			{
				MainFrame.Frame->data[0] = sourceplanes[0];
				MainFrame.Frame->linesize[0] = sourcestrides[0];
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
					MainFrame.Frame->data,
					MainFrame.Frame->linesize
				);
			}
		}

		void SendRawFrame()
		{
			MainFrame.Frame->pts = CurrentPresentation++;

			auto ret = avcodec_send_frame
			(
				CodecContext.Get(),
				MainFrame.Get()
			);

			ReceivePacketFrame();
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
			packet.stream_index = VideoStream->index;
			packet.duration = 1;

			av_packet_rescale_ts
			(
				&packet,
				CodecContext.Context->time_base,
				VideoStream->time_base
			);

			LAV::ThrowIfFailed
			(
				av_interleaved_write_frame
				(
					FormatContext.Get(),
					&packet
				)
			);
		}

		LAV::ScopedAVFContext FormatContext;
		
		AVCodec* Encoder = nullptr;
		LAV::ScopedCodecContext CodecContext;

		/*
			Conversion from RGB24 to whatever specified
		*/
		LAV::ScopedSWSContext FormatConverter;

		/*
			Incremented and written to for every sent frame
		*/
		int64_t CurrentPresentation = 0;
		AVStream* VideoStream = nullptr;
		LAV::ScopedAVCFrame MainFrame;
	};

	struct MovieData : public SDR::Sampler::IFramePrinter
	{
		std::unique_ptr<SDRVideoWriter> Video;

		bool IsStarted = false;

		std::string Name;

		uint32_t Width;
		uint32_t Height;

		uint32_t CurrentFrame = 0;
		uint32_t FinishedFrames = 0;

		double SamplingTime = 0.0;

		enum
		{
			/*
				Order: Blue Green Red
			*/
			BytesPerPixel = 3
		};

		auto GetImageSizeInBytes() const
		{
			return (Width * Height) * BytesPerPixel;
		}

		using BufferType = uint8_t;

		std::unique_ptr<SDR::Sampler::EasyByteSampler> Sampler;

		struct FutureSampleData
		{
			double Time;
			std::vector<BufferType> Data;
		};

		std::unique_ptr<moodycamel::ReaderWriterQueue<FutureSampleData>> FramesToSampleBuffer;
		std::thread FrameBufferThread;

		virtual void Print(BufferType* data) override;
	};

	MovieData CurrentMovie;
	std::atomic_bool ShouldStopBufferThread = false;

	void SDR_MovieShutdown()
	{
		if (!CurrentMovie.IsStarted)
		{
			return;
		}

		if (!ShouldStopBufferThread)
		{
			ShouldStopBufferThread = true;
			CurrentMovie.FrameBufferThread.join();
		}

		CurrentMovie = MovieData();
	}

	/*
		In case endmovie never gets called,
		this handles the plugin_unload
	*/
	SDR::PluginShutdownFunctionAdder A1(SDR_MovieShutdown);

	void MovieData::Print(BufferType* data)
	{
		/*
			The reason we override the default VID_ProcessMovieFrame is
			because that one creates a local CUtlBuffer for every frame and then destroys it.
			Better that we store them ourselves and iterate over the buffered frames to
			write them out in another thread.
		*/

		Video->SetRGB24Input(data, Width, Height);
		Video->SendRawFrame();
		FinishedFrames++;
	}
}

namespace
{
	namespace Variables
	{
		ConVar FrameRate
		(
			"sdr_render_framerate", "60", FCVAR_NEVER_AS_STRING,
			"Movie output framerate",
			true, 30, true, 1000
		);

		ConVar Exposure
		(
			"sdr_render_exposure", "1.0", FCVAR_NEVER_AS_STRING,
			"Frame exposure fraction",
			true, 0, true, 1
		);

		ConVar SamplesPerSecond
		(
			"sdr_render_samplespersecond", "600", FCVAR_NEVER_AS_STRING,
			"Game framerate in samples"
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
			"Where to save the output frames. UTF8 names are not supported in Source"
		);

		ConVar FlashWindow
		(
			"sdr_endmovieflash", "0", FCVAR_NEVER_AS_STRING,
			"Flash window when endmovie is called",
			true, 0, true, 1
		);

		namespace Video
		{
			ConVar CRF
			(
				"sdr_movie_encoder_crf", "10", 0,
				"Constant rate factor value. Values: 0 (best) - 51 (worst). See https://trac.ffmpeg.org/wiki/Encode/H.264"
			);

			ConVar Preset
			(
				"sdr_movie_encoder_preset", "medium", 0,
				"X264 encoder preset. See https://trac.ffmpeg.org/wiki/Encode/H.264"
			);

			ConVar Tune
			(
				"sdr_movie_encoder_tune", "", 0,
				"X264 encoder tune. See https://trac.ffmpeg.org/wiki/Encode/H.264"
			);
		}
	}

	void FrameBufferThreadHandler()
	{
		auto& interfaces = SDR::GetEngineInterfaces();
		auto& movie = CurrentMovie;
		auto& nonreadyframes = movie.FramesToSampleBuffer;

		auto sampleframerate = 1.0 / static_cast<double>(Variables::SamplesPerSecond.GetInt());

		MovieData::FutureSampleData sample;
		sample.Data.reserve(movie.GetImageSizeInBytes());

		while (!ShouldStopBufferThread)
		{
			while (nonreadyframes->try_dequeue(sample))
			{
				auto time = sample.Time;

				if (movie.Sampler->CanSkipConstant(time, sampleframerate))
				{
					movie.Sampler->Sample(nullptr, time);
				}

				else
				{
					auto data = sample.Data.data();
					movie.Sampler->Sample(data, time);
				}
			}
		}
	}

	#if 0
	namespace Module_BaseTemplateMask
	{
		auto Pattern = SDR_PATTERN("");
		auto Mask = "";

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"", "", Override, Pattern, Mask
		};

		void __cdecl Override()
		{

		}
	}

	namespace Module_BaseTemplateStatic
	{
		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleStaticAddress<ThisFunction> ThisHook
		{
			"", "", Override, 0x00000000
		};

		void __cdecl Override()
		{

		}
	}
	#endif

	namespace Module_StartMovie
	{
		/*
			0x100BCAC0 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN
		(
			"\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00"
			"\x00\x00\xD9\x45\x18\x56\x57\xF3\x0F\x10\x40\x00"
		);

		auto Mask = "xxxxx????x????xxxxxxxxx?";

		void __cdecl Override
		(
			const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CL_StartMovie", Override, Pattern, Mask
		};

		/*
			The 7th parameter (unk) was been added in Source 2013, it's not there in Source 2007
		*/
		void __cdecl Override
		(
			const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk
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
					Warning("SDR: Some unknown error happened when starting movie, related to sdr_outputdir\n");
					return;
				}
			}

			auto& movie = CurrentMovie;
				
			movie.Width = width;
			movie.Height = height;
			movie.Name = filename;

			{
				try
				{
					movie.Video = std::make_unique<SDRVideoWriter>();

					auto vidwriter = movie.Video.get();

					vidwriter->FormatConverter.Assign
					(
						width,
						height,
						AV_PIX_FMT_RGB24,
						AV_PIX_FMT_YUV420P
					);

					AVRational timebase;
					timebase.num = 1;
					timebase.den = Variables::FrameRate.GetInt();

					bool isx264;

					{
						auto targetpath = Variables::OutputDirectory.GetString();
						auto finalname = CUtlString::PathJoin(targetpath, filename);

						vidwriter->OpenFileForWrite(finalname);

						auto formatcontext = vidwriter->FormatContext.Get();
						auto oformat = formatcontext->oformat;

						if (strcmp(oformat->name, "image2") == 0)
						{
							isx264 = false;
							vidwriter->SetEncoder("png");
						}

						else
						{
							isx264 = true;
							vidwriter->SetEncoder("libx264");
						}
					}

					AVPixelFormat pxformat;

					vidwriter->VideoStream = vidwriter->AddStream();
					vidwriter->VideoStream->time_base = timebase;

					auto codeccontext = vidwriter->CodecContext.Get();
					codeccontext->codec_type = AVMEDIA_TYPE_VIDEO;
					codeccontext->width = width;
					codeccontext->height = height;
					codeccontext->time_base = timebase;

					if (isx264)
					{
						pxformat = AV_PIX_FMT_YUV420P;
						codeccontext->codec_id = AV_CODEC_ID_H264;
					}

					else
					{
						pxformat = AV_PIX_FMT_RGB24;
						codeccontext->codec_id = AV_CODEC_ID_PNG;
					}

					codeccontext->pix_fmt = pxformat;
		
					/*
						Not setting this will leave different colors across
						multiple programs
					*/
					codeccontext->colorspace = AVCOL_SPC_BT470BG;
					codeccontext->color_range = AVCOL_RANGE_MPEG;

					{
						if (isx264)
						{
							auto preset = Variables::Video::Preset.GetString();
							auto tune = Variables::Video::Tune.GetString();
							auto crf = Variables::Video::CRF.GetString();

							AVDictionary* options = nullptr;
							av_dict_set(&options, "preset", preset, 0);

							if (strlen(tune) > 0)
							{
								av_dict_set(&options, "tune", tune, 0);
							}

							av_dict_set(&options, "crf", crf, 0);

							vidwriter->OpenEncoder(&options);

							av_dict_free(&options);
						}

						else
						{
							vidwriter->OpenEncoder(nullptr);
						}
					}

					vidwriter->MainFrame.Assign
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
			auto hostframerate = g_pCVar->FindVar("host_framerate");
			hostframerate->SetValue(Variables::SamplesPerSecond.GetInt());

			auto framepitch = HLAE::CalcPitch(width, MovieData::BytesPerPixel, 1);
			auto movieframeratems = 1.0 / static_cast<double>(Variables::FrameRate.GetInt());
			auto moviexposure = Variables::Exposure.GetFloat();
			auto movieframestrength = Variables::FrameStrength.GetFloat();
				
			using SampleMethod = SDR::Sampler::EasySamplerSettings::Method;
			SampleMethod moviemethod;
				
			switch (Variables::SampleMethod.GetInt())
			{
				case 0:
				{
					moviemethod = SampleMethod::ESM_Rectangle;
					break;
				}

				case 1:
				{
					moviemethod = SampleMethod::ESM_Trapezoid;
					break;
				}
			}

			SDR::Sampler::EasySamplerSettings settings
			(
				MovieData::BytesPerPixel * width, height,
				moviemethod,
				movieframeratems,
				0.0,
				moviexposure,
				movieframestrength
			);

			using SDR::Sampler::EasyByteSampler;
			auto size = movie.GetImageSizeInBytes();

			movie.IsStarted = true;

			movie.Sampler = std::make_unique<EasyByteSampler>(settings, framepitch, &movie);

			movie.FramesToSampleBuffer = std::make_unique<moodycamel::ReaderWriterQueue<MovieData::FutureSampleData>>();

			ShouldStopBufferThread = false;
			movie.FrameBufferThread = std::thread(FrameBufferThreadHandler);
		}
	}

	namespace Module_CL_EndMovie
	{
		/*
			0x100BAE40 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN
		(
			"\x80\x3D\x00\x00\x00\x00\x00\x0F\x84\x00\x00\x00\x00"
			"\xD9\x05\x00\x00\x00\x00\x51\xB9\x00\x00\x00\x00"
		);

		auto Mask = "xx?????xx????xx????xx????";

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CL_EndMovie", Override, Pattern, Mask
		};

		void __cdecl Override()
		{
			if (!ShouldStopBufferThread)
			{
				ShouldStopBufferThread = true;
				CurrentMovie.FrameBufferThread.join();
			}

			/*
				Process all the delayed frames
			*/
			CurrentMovie.Video->SendFlushFrame();
			CurrentMovie.Video->WriteTrailer();

			SDR_MovieShutdown();

			ThisHook.GetOriginal()();

			auto hostframerate = g_pCVar->FindVar("host_framerate");
			hostframerate->SetValue(0);

			if (Variables::FlashWindow.GetBool())
			{
				auto& interfaces = SDR::GetEngineInterfaces();
				interfaces.EngineClient->FlashWindow();
			}
		}
	}

	namespace Module_CVideoMode_WriteMovieFrame
	{
		/*
			0x102011B0 static IDA address June 3 2016
		*/
		auto Pattern = SDR_PATTERN
		(
			"\x55\x8B\xEC\x51\x80\x3D\x00\x00\x00\x00\x00\x53"
			"\x8B\x5D\x08\x57\x8B\xF9\x8B\x83\x00\x00\x00\x00"
		);

		auto Mask = "xxxxxx?????xxxxxxxxx????";

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
			Static IDA addresses June 3 2016

			The purpose of overriding this function completely is to prevent the constant image buffer
			allocation that Valve does every movie frame. We just provide one buffer that gets reused.
		*/
		void __fastcall Override
		(
			void* thisptr, void* edx, void* info
		)
		{
			/*
				0x101FFF80 static IDA address June 3 2016
			*/
			static auto readscreenpxaddr = SDR::GetAddressFromPattern
			(
				"engine.dll",
				SDR_PATTERN
				(
					"\x55\x8B\xEC\x83\xEC\x14\x80\x3D\x00\x00\x00\x00\x00"
					"\x0F\x85\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00"
				),
				"xxxxxxxx?????xx????xx????"
			);

			using ReadScreenPxType = void(__fastcall*)
			(
				void*,
				void*,
				int x,
				int y,
				int w,
				int h,
				void* buffer,
				int format
			);

			static auto readscreenpxfunc = static_cast<ReadScreenPxType>(readscreenpxaddr);

			auto width = CurrentMovie.Width;
			auto height = CurrentMovie.Height;

			auto& movie = CurrentMovie;

			#ifdef _DEBUG
			Msg("SDR: Frame %d (%d)\n", movie.CurrentFrame, movie.FinishedFrames);
			#endif

			auto sampleframerate = 1.0 / static_cast<double>(Variables::SamplesPerSecond.GetInt());
			auto& time = movie.SamplingTime;

			MovieData::FutureSampleData newsample;
			newsample.Time = time;
			newsample.Data.resize(movie.GetImageSizeInBytes());

			/*
				3 = IMAGE_FORMAT_BGR888
				2 = IMAGE_FORMAT_RGB888
			*/
			readscreenpxfunc(thisptr, edx, 0, 0, width, height, newsample.Data.data(), 2);

			movie.FramesToSampleBuffer->enqueue(std::move(newsample));

			time += sampleframerate;
			movie.CurrentFrame++;
		}

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CVideoMode_WriteMovieFrame", Override, Pattern, Mask
		};
	}
}
