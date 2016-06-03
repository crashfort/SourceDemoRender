#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include "dbg.h"

#include "HLAE\HLAE.hpp"
#include "HLAE\Sampler.hpp"

#undef min
#undef max

namespace
{
	template <typename Str, typename... Format>
	void SDR_DebugMsg(Str message, Format... format)
	{
		#if _DEBUG
		Msg(message, format...);
		#endif
	}

	struct MovieData : public SDR::Sampler::IFramePrinter
	{
		size_t Width;
		size_t Height;

		size_t CurrentFrame = 0;
		size_t FinishedFrames = 0;

		double SamplingTime = 0.0;

		enum
		{
			/*
				Order: Blue Green Red
			*/
			BytesPerPixel = 3
		};

		size_t GetImageSizeInBytes() const
		{
			return (Width * Height) * BytesPerPixel;
		}

		using BufferType = unsigned char;

		std::unique_ptr<SDR::Sampler::EasyByteSampler> Sampler;

		BufferType* ActiveFrame;
		bool HasFrameDone = false;

		std::unique_ptr<BufferType[]> EngineFrameHeapData;

		virtual void Print(BufferType* data) override
		{
			ActiveFrame = data;
			HasFrameDone = true;
		}
	};

	MovieData CurrentMovie;
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

	namespace Module_CL_StartMovie
	{
		#define CALLING __cdecl
		#define RETURNTYPE void

		/*
			The 7th parameter (unk) was been added in Source 2013, it's not there in Source 2007
		*/
		#define PARAMETERS const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk
		using ThisFunction = RETURNTYPE(CALLING*)(PARAMETERS);
		#define CALLORIGINAL static_cast<ThisFunction>(ThisHook.GetOriginalFunction())

		/*
			0x100BCAC0 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x81\xEC\x00\x00\x00\x00\xA1\x00\x00\x00\x00\xD9\x45\x18\x56\x57\xF3\x0F\x10\x40\x00");
		auto Mask = "xxxxx????x????xxxxxxxxx?";

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CL_StartMovie",
			[](PARAMETERS)
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

				CALLORIGINAL(filename, flags, width, height, framerate, jpegquality, unk);

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

				CurrentMovie.Sampler = std::make_unique<SDR::Sampler::EasyByteSampler>(settings, framepitch, &CurrentMovie);

				CurrentMovie.EngineFrameHeapData = std::make_unique<MovieData::BufferType[]>(CurrentMovie.GetImageSizeInBytes());

			}, Pattern, Mask
		};
	}

	namespace Module_CL_EndMovie
	{
		#define CALLING __cdecl
		#define RETURNTYPE void
		#define PARAMETERS
		using ThisFunction = RETURNTYPE(CALLING*)(PARAMETERS);
		#define CALLORIGINAL static_cast<ThisFunction>(ThisHook.GetOriginalFunction())

		/*
			0x100BAE40 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x80\x3D\x00\x00\x00\x00\x00\x0F\x84\x00\x00\x00\x00\xD9\x05\x00\x00\x00\x00\x51\xB9\x00\x00\x00\x00");
		auto Mask = "xx?????xx????xx????xx????";

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CL_EndMovie",
			[](PARAMETERS)
			{
				CurrentMovie = MovieData();

				CALLORIGINAL();

				auto hostframerate = g_pCVar->FindVar("host_framerate");
				hostframerate->SetValue(0);

			}, Pattern, Mask
		};
	}

	namespace Module_VID_ProcessMovieFrame
	{
		#define CALLING __cdecl
		#define RETURNTYPE void

		/*
			First parameter (info) is a MovieInfo_t structure, but we don't need it
		*/
		#define PARAMETERS void* info, bool jpeg, const char* filename, int width, int height, unsigned char* data
		using ThisFunction = RETURNTYPE(CALLING*)(PARAMETERS);
		#define CALLORIGINAL static_cast<ThisFunction>(ThisHook.GetOriginalFunction())

		/*
			0x10201030 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x83\xEC\x30\x8D\x4D\xD0\x6A\x00\x6A\x00\x6A\x00\xE8\x00\x00\x00\x00");
		auto Mask = "xxxxxxxxxxxxxxxx????";

		/*
			Data passed to this is in BGR888 format
		*/
		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "VID_ProcessMovieFrame",
			[](PARAMETERS)
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

				if (movie.HasFrameDone)
				{
					CALLORIGINAL(info, jpeg, filename, width, height, movie.ActiveFrame);
					movie.HasFrameDone = false;
				}

				time += sampleframerate;
				movie.CurrentFrame++;

			}, Pattern, Mask
		};
	}

	namespace Module_CVideoMode_WriteMovieFrame
	{
		#define CALLING __fastcall
		#define RETURNTYPE void
		#define PARAMETERS void* thisptr, void* edx, void* info	
		using ThisFunction = RETURNTYPE(CALLING*)(PARAMETERS);
		#define CALLORIGINAL static_cast<ThisFunction>(ThisHook.GetOriginalFunction())

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
		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"engine.dll", "CVideoMode_WriteMovieFrame",
			[](PARAMETERS)
			{
				namespace ProcessModule = Module_VID_ProcessMovieFrame;
				using ProcessFrameType = ProcessModule::ThisFunction;

				static auto processframeaddr = ProcessModule::ThisHook.GetNewFunction();
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

				using ReadScreenPxType = void(__fastcall*)(void*, void*, int x, int y, int w, int h, void* buffer, int format);
				static auto readscreenpxfunc = static_cast<ReadScreenPxType>(readscreenpxaddr);

				auto buffer = CurrentMovie.EngineFrameHeapData.get();

				auto width = CurrentMovie.Width;
				auto height = CurrentMovie.Height;

				readscreenpxfunc(thisptr, edx, 0, 0, width, height, buffer, 3);

				processframefunc(info, false, nullptr, width, height, buffer);

			}, Pattern, Mask
		};
	}

	namespace Module_CBaseFileSystem_WriteFile
	{
		#define CALLING __fastcall
		#define RETURNTYPE bool
		#define PARAMETERS void* thisptr, void* edx, const char* filename, const char* path, CUtlBuffer& buffer
		using ThisFunction = RETURNTYPE(CALLING*)(PARAMETERS);
		#define CALLORIGINAL static_cast<ThisFunction>(ThisHook.GetOriginalFunction())

		/*
			0x1000E680 static IDA address May 22 2016
		*/
		auto Pattern = SDR_PATTERN("\x55\x8B\xEC\x53\x8B\x5D\x10\xBA\x00\x00\x00\x00\x57\x8B\xF9\x8A\x43\x15\xA8\x01");
		auto Mask = "xxxxxxxx????xxxxxxxx";

		/*
			Problem with this function is that "filename" and "path" are passed as UTF8, but
			are never converted to wchar on Windows in the engine. This limits all characters which is bad.
		*/
		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"FileSystem_Stdio.dll", "CBaseFileSystem_WriteFile",
			[](PARAMETERS)
			{
				if (CurrentMovie.HasFrameDone)
				{
					CUtlString frameformat;
					frameformat.Format("%s_%05d.tga", SDR_OutputName.GetString(), CurrentMovie.FinishedFrames);

					auto targetpath = SDR_OutputDirectory.GetString();
					auto filenameformat = CUtlString::PathJoin(targetpath, frameformat.Get());

					auto finalname = filenameformat.Get();

					auto ret = CALLORIGINAL(thisptr, edx, finalname, targetpath, buffer);

					if (ret)
					{
						CurrentMovie.FinishedFrames++;
					}

					return ret;
				}

				return CALLORIGINAL(thisptr, edx, filename, path, buffer);

			}, Pattern, Mask
		};
	}
}
