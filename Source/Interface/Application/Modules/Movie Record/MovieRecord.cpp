#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include "dbg.h"

#include "HLAE\HLAE.hpp"
#include "HLAE\Sampler.hpp"

#undef min
#undef max

namespace
{
	#ifdef _DEBUG
	#define SDR_DebugMsg Msg
	#else
	#define SDR_DebugMsg
	#endif

	struct MovieData : public SDR::Sampler::IFramePrinter
	{
		bool IsStarted = false;

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

		using BufferType = unsigned char;

		std::unique_ptr<SDR::Sampler::EasyByteSampler> Sampler;

		BufferType* ActiveFrame;
		bool HasFrameDone = false;

		std::unique_ptr<BufferType[]> EngineFrameHeapData;

		std::deque<CUtlBuffer> FramesToWriteBuffer;
		std::thread FrameBufferThread;

		virtual void Print(BufferType* data) override
		{
			ActiveFrame = data;
			HasFrameDone = true;
		}
	};

	MovieData CurrentMovie;

	std::atomic_bool ShouldStopBufferThread = false;
	std::atomic_bool ShouldPauseBufferThread = false;

	void MovieShutdown()
	{
		if (!CurrentMovie.IsStarted)
		{
			return;
		}

		if (!ShouldStopBufferThread)
		{
			ShouldStopBufferThread = true;
			CurrentMovie.FrameBufferThread.join();
			CurrentMovie = MovieData();
		}
	}

	/*
		In case endmovie never gets called,
		this hanldes the plugin_unload
	*/
	SDR::PluginShutdownFunctionAdder A1(MovieShutdown);
}

namespace
{
	ConVar SDR_FrameRate("sdr_render_framerate", "60", FCVAR_NEVER_AS_STRING, "Movie output framerate", true, 30, true, 1000);	

	ConVar SDR_Exposure("sdr_render_exposure", "1.0", FCVAR_NEVER_AS_STRING, "Frame exposure fraction", true, 0, true, 1);
	ConVar SDR_SamplesPerSecond("sdr_render_samplespersecond", "600", FCVAR_NEVER_AS_STRING, "Game framerate in samples");
	
	ConVar SDR_FrameStrength("sdr_render_framestrength", "1.0", FCVAR_NEVER_AS_STRING,
									"Controls clearing of the sampling buffer upon framing. "
									"The lower the value the more cross-frame motion blur",
									true, 0, true, 1);
	
	ConVar SDR_SampleMethod("sdr_render_samplemethod", "1", FCVAR_NEVER_AS_STRING,
								   "Selects the integral approximation method: "
								   "0: 1 point, rectangle method, 1: 2 point, trapezoidal rule",
								   true, 0, true, 1);

	ConVar SDR_OutputDirectory("sdr_outputdir", "", 0, "Where to save the output frames. UTF8 names are not supported in Source");
	ConVar SDR_OutputName("sdr_outputname", "", 0, "Prefix name of frames. UTF8 names are not supported in Source");

	ConVar SDR_FlashWindow("sdr_endmovieflash", "0", FCVAR_NEVER_AS_STRING, "Flash window when endmovie is called", true, 0, true, 1);

	void FrameBufferThreadHandler()
	{
		auto& interfaces = SDR::GetEngineInterfaces();
		auto& buffer = CurrentMovie.FramesToWriteBuffer;

		while (!ShouldStopBufferThread)
		{
			while (!buffer.empty())
			{
				while (ShouldPauseBufferThread)
				{
					std::this_thread::sleep_for(1ms);
				}

				auto& cur = buffer.front();

				CUtlString frameformat;
				frameformat.Format("%s_%05d.tga", SDR_OutputName.GetString(), CurrentMovie.FinishedFrames);

				auto targetpath = SDR_OutputDirectory.GetString();
				auto filenameformat = CUtlString::PathJoin(targetpath, frameformat.Get());

				auto finalname = filenameformat.Get();

				auto res = interfaces.FileSystem->WriteFile(finalname, targetpath, cur);

				/*
					Should probably handle this or something
				*/
				if (!res)
				{
					Warning("SDR: Could not write movie frame\n");
				}

				else
				{
					CurrentMovie.FinishedFrames++;
				}

				buffer.pop_front();
			}

			std::this_thread::sleep_for(500ms);
		}
	}

	#if 0
	namespace Module_BaseTemplateMask
	{
		auto Pattern = SDR_PATTERN("");
		auto Mask = "";

		template <typename T = void>
		void __cdecl Override()
		{
			
		}

		using ThisFunction = decltype(Override<>)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"", "", Override<>, Pattern, Mask
		};
	}

	namespace Module_BaseTemplateStatic
	{
		template <typename T = void>
		void __cdecl Override()
		{

		}

		using ThisFunction = decltype(Override<>)*;

		SDR::HookModuleStaticAddress<ThisFunction> ThisHook
		{
			"", "", Override<>, 0x00000000
		};
	}
	#endif

	namespace Module_StartMovie
	{
		/*
			0x100BCAC0 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\xD9\x45\x18\x56\x57\xF3\x0F\x10\x40\x00");
		auto Mask = "xxxxx????x????xxxxxxxxx?";

		/*
			The 7th parameter (unk) was been added in Source 2013, it's not there in Source 2007
		*/
		template <typename T = void>
		void __cdecl Override
		(
			const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk
		)
		{
			auto sdrpath = SDR_OutputDirectory.GetString();

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
				
			CurrentMovie.Width = width;
			CurrentMovie.Height = height;

			ThisHook.GetOriginal()(filename, flags, width, height, framerate, jpegquality, unk);

			/*
				The original function sets host_framerate to 30 so we override it
			*/
			auto hostframerate = g_pCVar->FindVar("host_framerate");
			hostframerate->SetValue(SDR_SamplesPerSecond.GetInt());

			auto framepitch = HLAE::CalcPitch(width, MovieData::BytesPerPixel, 1);
			auto movieframeratems = 1.0 / static_cast<double>(SDR_FrameRate.GetInt());
			auto moviexposure = SDR_Exposure.GetFloat();
			auto movieframestrength = SDR_FrameStrength.GetFloat();
				
			using SampleMethod = SDR::Sampler::EasySamplerSettings::Method;
			SampleMethod moviemethod;
				
			switch (SDR_SampleMethod.GetInt())
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

			CurrentMovie.IsStarted = true;

			CurrentMovie.Sampler = std::make_unique<SDR::Sampler::EasyByteSampler>(settings, framepitch, &CurrentMovie);

			CurrentMovie.EngineFrameHeapData = std::make_unique<MovieData::BufferType[]>(CurrentMovie.GetImageSizeInBytes());

			ShouldStopBufferThread = false;
			ShouldPauseBufferThread = false;
			CurrentMovie.FrameBufferThread = std::thread(FrameBufferThreadHandler);
		}

		using ThisFunction = decltype(Override<>)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CL_StartMovie", Override<>, Pattern, Mask
		};
	}

	namespace Module_CL_EndMovie
	{
		/*
			0x100BAE40 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x80\x3D\x00\x00\x00\x00\x00\x0F\x84\x00\x00\x00\x00\xD9\x05\x00\x00\x00\x00\x51\xB9\x00\x00\x00\x00");
		auto Mask = "xx?????xx????xx????xx????";

		template <typename T = void>
		void __cdecl Override()
		{
			MovieShutdown();

			ThisHook.GetOriginal()();

			auto hostframerate = g_pCVar->FindVar("host_framerate");
			hostframerate->SetValue(0);

			if (SDR_FlashWindow.GetBool())
			{
				auto& interfaces = SDR::GetEngineInterfaces();
				interfaces.EngineClient->FlashWindow();
			}
		}

		using ThisFunction = decltype(Override<>)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CL_EndMovie", Override<>, Pattern, Mask
		};
	}

	namespace Module_VID_ProcessMovieFrame
	{
		/*
			0x10201030 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x83\xEC\x30\x8D\x4D\xD0\x6A\x00\x6A\x00\x6A\x00\xE8\x00\x00\x00\x00");
		auto Mask = "xxxxxxxxxxxxxxxx????";

		/*
			First parameter (info) is a MovieInfo_t structure, but we don't need it

			Data passed to this is in BGR888 format
		*/
		template <typename T = void>
		void __cdecl Override
		(
			void* info, bool jpeg, const char* filename, int width, int height, unsigned char* data
		)
		{
			auto& movie = CurrentMovie;

			SDR_DebugMsg("SDR: Frame %d (%d)\n", movie.CurrentFrame, movie.FinishedFrames);

			auto sampleframerate = 1.0 / static_cast<double>(SDR_SamplesPerSecond.GetInt());
			auto& time = movie.SamplingTime;

			if (movie.Sampler->CanSkipConstant(time, sampleframerate))
			{
				movie.Sampler->Sample(nullptr, time);
			}

			else
			{
				movie.Sampler->Sample(data, time);
			}

			/*
				The reason we override the default VID_ProcessMovieFrame is
				because that one creates a local CUtlBuffer for every frame and then destroys it.
				Better that we store them ourselves and iterate over the buffered frames to
				write them out in another thread.
			*/
			if (movie.HasFrameDone)
			{
				movie.HasFrameDone = false;

				ShouldPauseBufferThread = true;

				/*
					A new empty buffer
				*/
				CurrentMovie.FramesToWriteBuffer.emplace_back();
				auto& newbuf = CurrentMovie.FramesToWriteBuffer.back();

				/*
					0x1022AD50 static IDA address June 4 2016
				*/
				static auto tgawriteraddr = SDR::GetAddressFromPattern
				(
					"engine.dll",
					SDR_PATTERN("\x55\x8B\xEC\x53\x57\x8B\x7D\x1C\x8B\xC7\x83\xE8\x00\x74\x0A\x83\xE8\x02\x75\x0A\x8D\x78\x03\xEB\x05\xBF\x00\x00\x00\x00"),
					"xxxxxxxxxxxxxxxxxxxxxxxxxx????"
				);

				using TGAWriterType = bool(__cdecl*)
				(
					unsigned char* data,
					CUtlBuffer& buffer,
					int width,
					int height,
					int srcformat,
					int dstformat
				);

				static auto tgawriterfunc = static_cast<TGAWriterType>(tgawriteraddr);

				/*
					3 = IMAGE_FORMAT_BGR888
					2 = IMAGE_FORMAT_RGB888
				*/
				auto res = tgawriterfunc(movie.ActiveFrame, newbuf, width, height, 3, 2);

				/*
					Should probably handle this or something
				*/
				if (!res)
				{
					Warning("SDR: Could not create TGA image\n");
				}
					
				ShouldPauseBufferThread = false;
			}

			time += sampleframerate;
			movie.CurrentFrame++;
		}

		using ThisFunction = decltype(Override<>)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "VID_ProcessMovieFrame", Override<>, Pattern, Mask
		};
	}

	namespace Module_CVideoMode_WriteMovieFrame
	{
		/*
			0x102011B0 static IDA address June 3 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x51\x80\x3D\x00\x00\x00\x00\x00\x53\x8B\x5D\x08\x57\x8B\xF9\x8B\x83\x00\x00\x00\x00");
		auto Mask = "xxxxxx?????xxxxxxxxx????";

		/*
			The "thisptr" in this context is a CVideoMode_MaterialSystem in this structure:
			
			CVideoMode_MaterialSystem
				CVideoMode_Common
					IVideoMode

			WriteMovieFrame belongs to CVideoMode_Common and ReadScreenPixels overriden by CVideoMode_MaterialSystem.
			The global engine variable "videomode" is of type CVideoMode_MaterialSystem which is what called WriteMovieFrame.
			
			For more usage see: VideoMode_Create (0x10201130) and VideoMode_Destroy (0x10201190)
			Static IDA addresses June 3 2016

			The purpose of overriding this function completely is to prevent the constant image buffer
			allocation that Valve does every movie frame. We just provide one buffer that gets reused.
		*/
		template <typename T = void>
		void __fastcall Override
		(
			void* thisptr, void* edx, void* info
		)
		{
			namespace ProcessModule = Module_VID_ProcessMovieFrame;
			using ProcessFrameType = ProcessModule::ThisFunction;

			static auto processframeaddr = ProcessModule::ThisHook.NewFunction;
			static auto processframefunc = static_cast<ProcessFrameType>(processframeaddr);

			/*
				0x101FFF80 static IDA address June 3 2016
			*/
			static auto readscreenpxaddr = SDR::GetAddressFromPattern
			(
				"engine.dll",
				SDR_PATTERN("\x55\x8B\xEC\x83\xEC\x14\x80\x3D\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00"),
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

			auto buffer = CurrentMovie.EngineFrameHeapData.get();

			auto width = CurrentMovie.Width;
			auto height = CurrentMovie.Height;

			/*
				3 = IMAGE_FORMAT_BGR888
				2 = IMAGE_FORMAT_RGB888
			*/
			readscreenpxfunc(thisptr, edx, 0, 0, width, height, buffer, 3);

			processframefunc(info, false, nullptr, width, height, buffer);
		}

		using ThisFunction = decltype(Override<>)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CVideoMode_WriteMovieFrame", Override<>, Pattern, Mask
		};
	}
}
