#include "PrecompiledHeader.hpp"
#include "MovieRecord.hpp"
#include <SDR Shared\Table.hpp>
#include "Interface\Application\Application.hpp"

#include "Various\Stream.hpp"
#include "Various\Audio.hpp"
#include <readerwriterqueue.h>
#include "Various\Profile.hpp"
#include "Interface\Application\Modules\Shared\EngineClient.hpp"
#include "Interface\Application\Modules\Shared\MaterialSystem.hpp"
#include "Interface\Application\Modules\Shared\SourceGlobals.hpp"
#include "Interface\LibraryInterface.hpp"
#include "Interface\Application\Extensions\ExtensionManager.hpp"

#include "Interface\Application\Modules\Shared\Console.hpp"

namespace
{
	namespace Variables
	{
		SDR::Console::Variable OutputDirectory;
		SDR::Console::Variable FlashWindow;
		SDR::Console::Variable ExitOnFinish;

		namespace Audio
		{
			SDR::Console::Variable OnlyAudio;
			SDR::Console::Variable DisableVideo;
		}

		namespace Video
		{
			SDR::Console::Variable Framerate;
			SDR::Console::Variable YUVColorSpace;
			SDR::Console::Variable Encoder;
			SDR::Console::Variable PixelFormat;

			namespace LAV
			{
				SDR::Console::Variable SuppressLog;
			}

			namespace Sample
			{
				SDR::Console::Variable Multiply;
				SDR::Console::Variable Exposure;
			}

			namespace D3D11
			{
				SDR::Console::Variable Staging;
			}

			namespace X264
			{
				SDR::Console::Variable CRF;
				SDR::Console::Variable Preset;
				SDR::Console::Variable Intra;
			}
		}

		/*
			Creation has to be delayed as the necessary console stuff isn't available earlier.
		*/
		SDR::StartupFunctionAdder A1("MovieRecord console variables", []()
		{
			OutputDirectory = SDR::Console::MakeString("sdr_outputdir", "");
			FlashWindow = SDR::Console::MakeBool("sdr_endmovieflash", "0");
			ExitOnFinish = SDR::Console::MakeBool("sdr_endmoviequit", "0");

			Video::Framerate = SDR::Console::MakeNumber("sdr_video_fps", "60", 30, 1000);
			Video::YUVColorSpace = SDR::Console::MakeString("sdr_video_yuvspace", "709");
			Video::Encoder = SDR::Console::MakeString("sdr_video_encoder", "libx264rgb");
			Video::PixelFormat = SDR::Console::MakeString("sdr_video_pxformat", "");

			Video::LAV::SuppressLog = SDR::Console::MakeBool("sdr_video_lav_suppresslog", "1");

			Video::Sample::Multiply = SDR::Console::MakeNumber("sdr_video_sample_mult", "32", 0);
			Video::Sample::Exposure = SDR::Console::MakeNumber("sdr_video_sample_exposure", "0.5", 0, 1);

			Video::D3D11::Staging = SDR::Console::MakeBool("sdr_video_d3d11_staging", "1");

			Video::X264::CRF = SDR::Console::MakeNumberWithString("sdr_video_x264_crf", "0", 0, 51);
			Video::X264::Preset = SDR::Console::MakeString("sdr_video_x264_preset", "ultrafast");
			Video::X264::Intra = SDR::Console::MakeBool("sdr_video_x264_intra", "1");

			Audio::OnlyAudio = SDR::Console::MakeBool("sdr_audio_only", "0");
			Audio::DisableVideo = SDR::Console::MakeBool("sdr_audio_disable_video", "1");
		});
	}
}

namespace
{	
	void LAVLogFunction(void* avcl, int level, const char* fmt, va_list vl)
	{
		if (!Variables::Video::LAV::SuppressLog.GetBool())
		{
			/*
				989 max limit according to
				https://developer.valvesoftware.com/wiki/Developer_Console_Control#Printing_to_the_console

				960 to keep in a 32 byte alignment.
			*/
			char buf[960];
			vsprintf_s(buf, fmt, vl);

			/*
				Not formatting the buffer to a string will create a runtime error on any float conversion.
			*/
			SDR::Log::Message("%s", buf);
		}
	}
}

namespace
{
	std::atomic_int32_t BufferedItems;
	std::atomic_bool ShouldStopThread;

	struct MovieData
	{
		bool IsStarted = false;

		int OldMatQueueModeValue;
		int OldEngineSleepTime;
		
		float OldAudioMixAhead;

		/*
			Whether to use an extra intermediate buffer for GPU -> CPU transfer.
		*/
		static bool UseStaging()
		{
			return Variables::Video::D3D11::Staging.GetBool();
		}

		static bool UseSampling()
		{
			auto exposure = Variables::Video::Sample::Exposure.GetFloat();
			auto mult = Variables::Video::Sample::Multiply.GetInt();

			return mult > 1 && exposure > 0;
		}

		static bool WouldNewFrameOverflow()
		{
			PROCESS_MEMORY_COUNTERS desc = {};

			auto res = K32GetProcessMemoryInfo(GetCurrentProcess(), &desc, sizeof(desc));

			if (res == 0)
			{
				SDR::Log::Warning("SDR: Could not retrieve process memory info\n");
				return true;
			}

			return desc.WorkingSetSize > INT32_MAX;
		}

		void VideoThread()
		{
			SDR::Stream::FutureData videoitem;

			while (!ShouldStopThread)
			{
				while (VideoQueue->try_dequeue(videoitem))
				{
					--BufferedItems;

					videoitem.Writer->SetFrameInput(videoitem.Planes);
					videoitem.Writer->SendRawFrame();
				}
			}
		}

		void AudioThread()
		{
			std::vector<int16_t> audioitem;

			while (!ShouldStopThread)
			{
				while (AudioQueue->try_dequeue(audioitem))
				{
					--BufferedItems;

					AudioWriter->WritePCM16Samples(audioitem);
				}
			}
		}
	
		template <typename T>
		void CreateProcessingThread(T func)
		{
			BufferedItems = 0;
			ShouldStopThread = false;

			ThreadHandle = std::thread(func);
		}

		void SetVideoThread()
		{
			CreateProcessingThread(std::bind(&MovieData::VideoThread, this));
		}

		void SetAudioThread()
		{
			CreateProcessingThread(std::bind(&MovieData::AudioThread, this));
		}

		static void WaitForBufferedItems()
		{
			/*
				Don't risk running out of memory. Just let the encoding finish so we start fresh with no buffered frames.
			*/
			if (MovieData::WouldNewFrameOverflow())
			{
				while (BufferedItems)
				{
					std::this_thread::sleep_for(1ms);
				}
			}
		}

		struct
		{
			bool Enabled;
			float Exposure;

			double TimePerSample;
			double TimePerFrame;
		} SamplingData;

		/*
			Skip first frame as it will always be black when capturing the engine backbuffer.
		*/
		bool FirstFrame = true;

		SDR::Stream::SharedData VideoStreamShared;
		
		std::unique_ptr<SDR::Stream::QueueType> VideoQueue;
		std::unique_ptr<SDR::Stream::StreamBase> VideoStream;

		using AudioQueueType = moodycamel::ReaderWriterQueue<std::vector<int16_t>>;
		
		std::unique_ptr<AudioQueueType> AudioQueue;
		std::unique_ptr<SDR::Audio::Writer> AudioWriter;

		std::thread ThreadHandle;
	};

	MovieData CurrentMovie;
}

bool SDR::MovieRecord::ShouldRecordVideo()
{
	if (!CurrentMovie.IsStarted)
	{
		return false;
	}

	if (SDR::SourceGlobals::IsDrawingLoading())
	{
		return false;
	}

	if (SDR::EngineClient::IsConsoleVisible())
	{
		return false;
	}

	if (CurrentMovie.AudioWriter)
	{
		return false;
	}

	return true;
}

namespace
{
	namespace ModuleView_Render
	{
		namespace Common
		{
			bool CopyDX9ToDX11(SDR::Stream::StreamBase* stream)
			{
				auto dx9device = SDR::SourceGlobals::GetD3D9DeviceEx();

				auto rt = stream->DirectX9.GameRenderTarget0.Get();
				auto surface = stream->DirectX9.SharedSurface.Surface.Get();

				/*
					The DX11 texture now contains this data.
				*/
				auto hr = dx9device->StretchRect(rt, nullptr, surface, nullptr, D3DTEXF_NONE);

				if (FAILED(hr))
				{
					SDR::Log::Warning("SDR: Could not copy D3D9 RT -> D3D11 RT\n");
					return false;
				}

				return true;
			}

			void Pass(SDR::Stream::StreamBase* stream)
			{
				if (!CopyDX9ToDX11(stream))
				{
					return;
				}

				auto& sampling = CurrentMovie.SamplingData;

				auto save = [=]()
				{
					SDR::Stream::FutureData item;
					item.Writer = &stream->Video;

					stream->DirectX11.NewVideoFrame(CurrentMovie.VideoStreamShared);
					stream->DirectX11.Conversion(CurrentMovie.VideoStreamShared);
					auto res = stream->DirectX11.Download(CurrentMovie.VideoStreamShared, item);

					if (res)
					{
						++BufferedItems;
						CurrentMovie.VideoQueue->enqueue(std::move(item));
					}
				};

				/*
					When enough frames have been sampled to form a total weight of 1, it will print the final frame.
				*/
				if (sampling.Enabled)
				{
					auto proc = [=](float weight)
					{
						auto& shared = CurrentMovie.VideoStreamShared;
						stream->DirectX11.NewFrame(shared, weight);
					};

					auto clear = [=]()
					{
						stream->DirectX11.Clear(CurrentMovie.VideoStreamShared);
					};

					auto& rem = stream->SamplingData.Remainder;
					auto oldrem = rem;
					auto exposure = sampling.Exposure;

					rem += sampling.TimePerSample / sampling.TimePerFrame;

					/*
						Cast to float to prevent comparisons against 0.99999999998...
					*/

					if ((float)rem <= (1.0 - exposure))
					{

					}

					else if ((float)rem < 1.0)
					{
						auto weight = (rem - std::max(1.0 - exposure, oldrem)) * (1.0 / exposure);
						proc(weight);
					}

					else
					{
						auto weight = (1.0 - std::max(1.0 - exposure, oldrem)) * (1.0 / exposure);

						proc(weight);
						save();

						rem -= 1.0;

						uint32_t additional = rem;

						if (additional > 0)
						{
							for (int i = 0; i < additional; i++)
							{
								save();
							}

							rem -= additional;
						}

						clear();

						if (rem > FLT_EPSILON && rem > (1.0 - exposure))
						{
							weight = ((rem - (1.0 - exposure)) * (1.0 / exposure));
							proc(weight);
						}
					}
				}

				/*
					No sampling, just pass through as is to conversion shader and save.
				*/
				else
				{
					stream->DirectX11.Pass(CurrentMovie.VideoStreamShared);
					save();
				}
			}

			void Procedure()
			{
				bool dopasses = SDR::MovieRecord::ShouldRecordVideo();

				if (dopasses)
				{
					MovieData::WaitForBufferedItems();

					if (CurrentMovie.FirstFrame)
					{
						CurrentMovie.FirstFrame = false;
						CopyDX9ToDX11(CurrentMovie.VideoStream.get());
					}

					else
					{
						Pass(CurrentMovie.VideoStream.get());
					}
				}
			}
		}

		namespace Variant0
		{
			void __fastcall NewFunction(void* thisptr, void* edx, void* rect);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __fastcall NewFunction(void* thisptr, void* edx, void* rect)
			{
				if (!CurrentMovie.IsStarted)
				{
					ThisHook.GetOriginal()(thisptr, edx, rect);
					return;
				}

				if (Variables::Audio::DisableVideo.GetBool())
				{
					if (!SDR::EngineClient::IsConsoleVisible() && !SDR::SourceGlobals::IsDrawingLoading())
					{
						if (CurrentMovie.AudioWriter)
						{
							return;
						}
					}
				}

				ThisHook.GetOriginal()(thisptr, edx, rect);
				Common::Procedure();
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"View_Render",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleStartMovie
	{
		int Variant;

		namespace Common
		{
			void VerifyOutputDirectory(const std::string& path)
			{
				char final[SDR::File::NameSize];
				strcpy_s(final, path.c_str());

				PathAddBackslashA(final);

				auto winstr = SDR::String::FromUTF8(final);

				auto res = PathFileExistsW(winstr.c_str()) == 1;

				if (!res)
				{
					SDR::Error::Microsoft::ThrowLastError("Could not access wanted output directory");
				}
			}

			std::string BuildVideoStreamName(const std::string& savepath, const char* filename)
			{
				char finalname[SDR::File::NameSize];

				PathCombineA(finalname, savepath.c_str(), filename);

				return { finalname };
			}

			std::wstring BuildAudioStreamName(const std::string& savepath, const char* filename)
			{
				wchar_t finalname[SDR::File::NameSize];

				auto savepathw = SDR::String::FromUTF8(savepath);
				auto filenamew = SDR::String::FromUTF8(filename);

				PathCombineW(finalname, savepathw.c_str(), filenamew.c_str());
				PathRenameExtensionW(finalname, L".wav");

				return { finalname };
			}

			void WarnAboutVariableValues()
			{
				auto encoderstr = Variables::Video::Encoder.GetString();
				auto encoder = avcodec_find_encoder_by_name(encoderstr);

				if (!encoder)
				{
					SDR::Log::Warning("SDR: Encoder \"%s\" not found, available encoders:\n", encoderstr);

					auto next = av_codec_next(nullptr);

					while (next)
					{
						SDR::Log::Message("SDR: %s\n", next->name);
						next = av_codec_next(next);
					}

					SDR::Error::Make("Encoder not found"s);
				}

				else
				{
					if (encoder->id == AV_CODEC_ID_H264)
					{
						auto newstr = Variables::Video::X264::Preset.GetString();

						auto slowpresets = { "slow", "slower", "veryslow", "placebo" };

						for (auto preset : slowpresets)
						{
							if (_strcmpi(newstr, preset) == 0)
							{
								SDR::Log::Warning("SDR: Slow encoder preset chosen, this might not work very well for realtime\n");
								break;
							}
						}
					}
				}
			}

			void WarnAboutName(const char* filename)
			{
				/*
					https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx
				*/

				auto symbols = { L'<', L'>', L':', L'\"', L'/', L'\\', L'|', L'?', L'*' };

				auto names =
				{
					L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7",
					L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
				};

				auto wstr = SDR::String::FromUTF8(filename);
				auto onlyname = wstr.substr(0, wstr.find_first_of(L'.'));

				for (auto symbol : symbols)
				{
					if (onlyname.find(symbol) != std::wstring::npos)
					{
						SDR::Error::Make("File has illegal symbol: \"%c\"", symbol);
					}
				}

				for (auto reserved : names)
				{
					if (_wcsicmp(reserved, onlyname.c_str()) == 0)
					{
						auto display = SDR::String::ToUTF8(reserved);
						SDR::Error::Make("File has reserved name: \"%s\"", display.c_str());
					}
				}

				Microsoft::WRL::Wrappers::HandleT<Microsoft::WRL::Wrappers::HandleTraits::HANDLETraits> file;
				file.Attach(CreateFileW(wstr.c_str(), 0, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, nullptr));

				if (!file.IsValid())
				{
					SDR::Error::Microsoft::ThrowLastError("Could not create file, name probably invalid");
				}
			}

			void WarnAboutExtension(const char* filename)
			{
				auto wstr = SDR::String::FromUTF8(filename);				
				auto ext = PathFindExtensionW(wstr.c_str());

				auto containers = { ".avi", ".mp4", ".mov", ".mkv" };

				auto showcontainers = [&]()
				{
					for (auto type : containers)
					{
						SDR::Log::Message("SDR: %s\n", type);
					}
				};

				if (*ext == 0)
				{
					SDR::Log::Warning("SDR: No file extension. Available video containers:\n");

					showcontainers();

					SDR::Error::Make("Missing file extension"s);
				}

				else
				{
					auto extutf8 = SDR::String::ToUTF8(ext);
					bool found = false;

					for (auto type : containers)
					{
						if (_strcmpi(type, extutf8.c_str()) == 0)
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						SDR::Log::Warning("SDR: Unknown file extension. Available video containers:\n");

						showcontainers();

						SDR::Error::Make("Unknown file extension"s);
					}
				}
			}

			std::string TrimFileName(const char* filename)
			{
				std::string newname = filename;

				if (SDR::String::IsSpace(newname.back()))
				{
					for (auto it = newname.rbegin(); it != newname.rend(); ++it)
					{
						if (SDR::String::IsSpace(*it))
						{
							newname.pop_back();
						}

						else
						{
							break;
						}
					}
				}

				if (SDR::String::IsSpace(newname.front()))
				{
					newname = newname.substr(newname.find_first_not_of(' ', 0));
				}

				return newname;
			}

			std::string GetOutputDirectory()
			{
				std::string ret = Variables::OutputDirectory.GetString();

				/*
					No desired path, use resource root.
				*/
				if (ret.empty())
				{
					ret = SDR::Library::GetResourcePath();
				}

				else
				{
					VerifyOutputDirectory(ret);
				}

				return ret;
			}

			void AudioProcedure(const char* name)
			{
				CurrentMovie.AudioWriter = std::make_unique<SDR::Audio::Writer>();

				auto sdrpath = ModuleStartMovie::Common::GetOutputDirectory();
				auto audioname = ModuleStartMovie::Common::BuildAudioStreamName(sdrpath, name);

				/*
					This is the only supported format.
				*/
				CurrentMovie.AudioWriter->Open(audioname.c_str(), 44'100, 16, 2);

				/*
					Ensure minimal audio latency.
				*/
				CurrentMovie.OldAudioMixAhead = SDR::Console::Variable::SetValueGetOld<float>("snd_mixahead", 0);

				/*
					Allow fast processing when game window is not focused.
				*/
				CurrentMovie.OldEngineSleepTime = SDR::Console::Variable::SetValueGetOld<int>("engine_no_focus_sleep", 0);

				CurrentMovie.AudioQueue = std::make_unique<MovieData::AudioQueueType>(256);
					
				CurrentMovie.SetAudioThread();
			}

			void VideoProcedure(const char* filename, int width, int height)
			{
				WarnAboutExtension(filename);
				WarnAboutVariableValues();

				auto sdrpath = GetOutputDirectory();

				/*
					Default to 709 space and full range.
				*/
				auto colorspace = AVCOL_SPC_BT709;
				auto colorrange = AVCOL_RANGE_JPEG;
				auto pxformat = AV_PIX_FMT_NONE;

				{
					auto table =
					{
						std::make_pair("601", AVCOL_SPC_BT470BG),
						std::make_pair("709", AVCOL_SPC_BT709)
					};

					SDR::Table::LinkToVariable(Variables::Video::YUVColorSpace.GetString(), table, colorspace);
				}

				if (Variables::Video::LAV::SuppressLog.GetBool())
				{
					av_log_set_level(AV_LOG_QUIET);
					av_log_set_callback(nullptr);
				}

				else
				{
					av_log_set_level(AV_LOG_INFO);
					av_log_set_callback(LAVLogFunction);
				}

				CurrentMovie.VideoStream = std::make_unique<SDR::Stream::StreamBase>();

				struct VideoConfigurationData
				{
					using FormatsType = std::vector<std::pair<const char*, AVPixelFormat>>;

					static VideoConfigurationData Make(const char* name, FormatsType&& formats)
					{
						VideoConfigurationData ret;
						ret.Encoder = avcodec_find_encoder_by_name(name);
						ret.PixelFormats = std::move(formats);

						return ret;
					}

					AVCodec* Encoder;
					FormatsType PixelFormats;
				};

				auto yuv420 = std::make_pair("yuv420", AV_PIX_FMT_YUV420P);
				auto yuv444 = std::make_pair("yuv444", AV_PIX_FMT_YUV444P);
				auto bgr0 = std::make_pair("bgr0", AV_PIX_FMT_BGR0);

				VideoConfigurationData table[] =
				{
					VideoConfigurationData::Make("libx264", { yuv420, yuv444 }),
					VideoConfigurationData::Make("libx264rgb", { bgr0 }),
				};

				VideoConfigurationData* vidconfig = nullptr;

				{
					auto encoderstr = Variables::Video::Encoder.GetString();
					auto encoder = avcodec_find_encoder_by_name(encoderstr);

					SDR::Error::ThrowIfNull(encoder, "Video encoder \"%s\" not found", encoderstr);

					for (auto& config : table)
					{
						if (config.Encoder == encoder)
						{
							vidconfig = &config;
							break;
						}
					}

					auto pxformatstr = Variables::Video::PixelFormat.GetString();

					if (!SDR::Table::LinkToVariable(pxformatstr, vidconfig->PixelFormats, pxformat))
					{
						/*
							User selected pixel format does not match any in config.
						*/
						pxformat = vidconfig->PixelFormats[0].second;
					}

					auto isrgbtype = [](AVPixelFormat format)
					{
						auto table = { AV_PIX_FMT_BGR0 };

						for (auto entry : table)
						{
							if (format == entry)
							{
								return true;
							}
						}

						return false;
					};

					if (isrgbtype(pxformat))
					{
						colorspace = AVCOL_SPC_RGB;
						colorrange = AVCOL_RANGE_UNSPECIFIED;
					}

					CurrentMovie.VideoStream->Video.Frame.Assign(width, height, pxformat, colorspace, colorrange);

					CurrentMovie.VideoStreamShared.DirectX11.Create(width, height, MovieData::UseSampling());

					CurrentMovie.VideoStream->DirectX9.Create(SDR::SourceGlobals::GetD3D9DeviceEx(), width, height);

					{
						auto device = CurrentMovie.VideoStreamShared.DirectX11.Device.Get();
						auto sharedhandle = CurrentMovie.VideoStream->DirectX9.SharedSurface.SharedHandle;
						auto frame = CurrentMovie.VideoStream->Video.Frame.Get();
						auto staging = MovieData::UseStaging();

						CurrentMovie.VideoStream->DirectX11.Create(device, sharedhandle, frame, staging);
					}

					/*
						Destroy any deferred D3D11 resources created by above functions.
					*/
					CurrentMovie.VideoStreamShared.DirectX11.Context->Flush();

					auto name = BuildVideoStreamName(sdrpath, filename);

					CurrentMovie.VideoStream->Video.OpenFileForWrite(name.c_str());
					CurrentMovie.VideoStream->Video.SetEncoder(vidconfig->Encoder);
				}

				{
					SDR::LAV::ScopedAVDictionary options;

					if (vidconfig->Encoder->id == AV_CODEC_ID_H264)
					{
						namespace X264 = Variables::Video::X264;

						auto preset = X264::Preset.GetString();
						auto crf = X264::CRF.GetString();
						auto intra = X264::Intra.GetBool();

						options.Set("preset", preset);
						options.Set("crf", crf);

						if (intra)
						{
							/*
								Setting every frame as a keyframe gives the ability to use the video in a video editor with ease.
							*/
							options.Set("x264-params", "keyint=1");
						}
					}

					auto fps = Variables::Video::Framerate.GetInt();
					
					CurrentMovie.VideoStream->Video.OpenEncoder(fps, options.Get());
					CurrentMovie.VideoStream->Video.WriteHeader();
				}

				/*
					Don't call the original CL_StartMovie as it causes major recording slowdowns.
				*/

				auto fps = Variables::Video::Framerate.GetInt();
				auto exposure = Variables::Video::Sample::Exposure.GetFloat();
				auto mult = Variables::Video::Sample::Multiply.GetInt();

				auto enginerate = fps;

				if (MovieData::UseSampling())
				{
					enginerate *= mult;
				}

				SDR::Console::Variable::SetValue("host_framerate", enginerate);

				/*
					Force single threaded processing or else there will be flickering.
				*/
				CurrentMovie.OldMatQueueModeValue = SDR::Console::Variable::SetValueGetOld<int>("mat_queue_mode", 0);

				/*
					Allow fast processing when game window is not focused.
				*/
				CurrentMovie.OldEngineSleepTime = SDR::Console::Variable::SetValueGetOld<int>("engine_no_focus_sleep", 0);

				CurrentMovie.SamplingData.Enabled = MovieData::UseSampling();

				CurrentMovie.SamplingData.Exposure = exposure;
				CurrentMovie.SamplingData.TimePerSample = 1.0 / enginerate;
				CurrentMovie.SamplingData.TimePerFrame = 1.0 / fps;

				CurrentMovie.VideoQueue = std::make_unique<SDR::Stream::QueueType>(256);

				CurrentMovie.SetVideoThread();

				{
					SDR::Extension::StartMovieData data;
					data.Device = CurrentMovie.VideoStreamShared.DirectX11.Device.Get();
					
					data.Width = width;
					data.Height = height;
					
					data.FrameRate = fps;
					data.HostFrameRate = enginerate;
					data.TimePerFrame = 1.0 / fps;
					data.TimePerSample = 1.0 / enginerate;

					SDR::ExtensionManager::Events::StartMovie(data);
				}
			}
		}

		namespace Variant0
		{
			/*
				The 7th parameter (unk) was been added in Source 2013, it's not there in Source 2007.
			*/
			void __cdecl NewFunction(const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void CallOriginalForAudio()
			{
				auto fps = Variables::Video::Framerate.GetInt();

				/*
					Value 4 means WAV audio flag. The original function is only called with audio processing.
				*/
				ThisHook.GetOriginal()("x", 4, 0, 0, fps, 0, 0);
			}

			void __cdecl NewFunction(const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk)
			{
				
			}
		}

		void CallOriginalForAudio()
		{
			if (Variant == 0)
			{
				Variant0::CallOriginalForAudio();
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"StartMovie",
				[](const rapidjson::Value& value)
				{
					Variant = SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleStartMovieCommand
	{
		namespace Common
		{
			/*
				This command is overriden to remove the incorrect description.
			*/
			void Procedure(const void* ptr)
			{
				SDR::Console::CommandArgs args(ptr);

				if (CurrentMovie.IsStarted)
				{
					SDR::Log::Message("SDR: Processing is already started\n");
					return;
				}

				if (args.Count() < 2)
				{
					SDR::Log::Message("SDR: Name is required for \"startmovie\", see GitHub page for help\n");
					return;
				}

				int width;
				int height;
				SDR::MaterialSystem::GetBackBufferDimensions(width, height);

				SDR::Profile::Reset();

				auto name = args.FullValue();

				auto newname = ModuleStartMovie::Common::TrimFileName(name);
				name = newname.c_str();

				CurrentMovie = {};

				try
				{
					ModuleStartMovie::Common::WarnAboutName(name);

					if (Variables::Audio::OnlyAudio.GetBool())
					{
						SDR::Log::Message("SDR: Started audio processing\n");

						ModuleStartMovie::Common::AudioProcedure(name);
						ModuleStartMovie::CallOriginalForAudio();
					}

					else
					{
						SDR::Log::Message("SDR: Started video processing\n");

						ModuleStartMovie::Common::VideoProcedure(name, width, height);
					}

					CurrentMovie.IsStarted = true;
				}

				catch (const SDR::Error::Exception& error)
				{
					SDR::Log::Warning("SDR: Could not start processing\n");

					CurrentMovie = {};
				}
			}
		}

		namespace Variant0
		{
			void __cdecl NewFunction(const void* ptr);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(const void* ptr)
			{
				Common::Procedure(ptr);
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"StartMovieCommand",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleEndMovie
	{
		namespace Common
		{
			void Procedure()
			{
				if (!CurrentMovie.IsStarted)
				{
					SDR::Log::Message("SDR: No processing is started\n");
					return;
				}

				CurrentMovie.IsStarted = false;

				/*
					Don't call original function as we don't call the engine's startmovie.
				*/

				SDR::Log::Message("SDR: Ending processing\n");

				if (CurrentMovie.AudioWriter)
				{
					if (!ShouldStopThread)
					{
						ShouldStopThread = true;
						CurrentMovie.ThreadHandle.join();
					}

					CurrentMovie.AudioWriter->Finish();

					SDR::Console::Variable::SetValue("snd_mixahead", CurrentMovie.OldAudioMixAhead);
				}

				else
				{
					if (BufferedItems > 0)
					{
						SDR::Log::Message("SDR: There are %d buffered frames remaining\n", BufferedItems.load());
					}

					if (!ShouldStopThread)
					{
						ShouldStopThread = true;
						CurrentMovie.ThreadHandle.join();
					}

					SDR::Console::Variable::SetValue("mat_queue_mode", CurrentMovie.OldMatQueueModeValue);

					/*
						Let the encoder finish all the delayed frames.
					*/
					CurrentMovie.VideoStream->Video.Finish();

					SDR::ExtensionManager::Events::EndMovie();
				}

				SDR::Console::Variable::SetValue("host_framerate", 0);
				SDR::Console::Variable::SetValue("engine_no_focus_sleep", CurrentMovie.OldEngineSleepTime);

				CurrentMovie = {};

				if (Variables::ExitOnFinish.GetBool())
				{
					SDR::EngineClient::ClientCommand("quit\n");
					return;
				}

				if (Variables::FlashWindow.GetBool())
				{
					SDR::EngineClient::FlashWindow();
				}

				SDR::Log::MessageColor({ 88, 255, 39 }, "SDR: Processing is now complete\n");

				SDR::Profile::ShowResults();
			}
		}

		namespace Variant0
		{
			void __cdecl NewFunction();

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void CallOriginal()
			{
				ThisHook.GetOriginal()();
			}

			void __cdecl NewFunction()
			{
				
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"EndMovie",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleEndMovieCommand
	{
		namespace Common
		{
			/*
				Always allow ending movie.
			*/
			void Procedure()
			{
				/*
					Ending movie must be synchronous with extensions.
				*/
				ModuleEndMovie::Common::Procedure();
			}
		}

		namespace Variant0
		{
			void __cdecl NewFunction();

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction()
			{
				Common::Procedure();
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"EndMovieCommand",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleSUpdateGuts
	{
		namespace Variant0
		{
			void __cdecl NewFunction(float mixahead);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(float mixahead)
			{
				if (!CurrentMovie.IsStarted)
				{
					ThisHook.GetOriginal()(mixahead);
					return;
				}

				if (CurrentMovie.AudioWriter)
				{
					if (CurrentMovie.FirstFrame)
					{
						CurrentMovie.FirstFrame = false;
						return;
					}

					ThisHook.GetOriginal()(0);
				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"SUpdateGuts",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleVideoMode
	{
		void* Ptr;

		namespace WriteMovieFrame
		{
			namespace Variant0
			{
				void __fastcall NewFunction(void* thisptr, void* edx, void* info);

				using OverrideType = decltype(NewFunction)*;
				SDR::Hooking::HookModule<OverrideType> ThisHook;

				void __fastcall NewFunction(void* thisptr, void* edx, void* info)
				{

				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"VideoModePtr",
				[](const rapidjson::Value& value)
				{
					auto address = SDR::Hooking::GetAddressFromJsonPattern(value);
					
					Ptr = **(void***)address;
					SDR::Error::ThrowIfNull(Ptr);

					SDR::Hooking::ModuleShared::Registry::SetKeyValue("VideoModePtr", Ptr);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"VideoMode_WriteMovieFrame",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(WriteMovieFrame::Variant0::ThisHook, WriteMovieFrame::Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleWaveCreateTmpFile
	{
		namespace Variant0
		{
			void __cdecl NewFunction(const char* filename, int rate, int bits, int channels);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(const char* filename, int rate, int bits, int channels)
			{
				
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WaveCreateTmpFile",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleWaveAppendTmpFile
	{
		namespace Variant0
		{
			void __cdecl NewFunction(const char* filename, void* buffer, int samplebits, int samplecount);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(const char* filename, void* buffer, int samplebits, int samplecount)
			{
				MovieData::WaitForBufferedItems();

				auto start = static_cast<int16_t*>(buffer);
				auto length = samplecount * samplebits / 8;

				++BufferedItems;
				CurrentMovie.AudioQueue->emplace(start, start + length);
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WaveAppendTmpFile",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}

	namespace ModuleWaveFixupTmpFile
	{
		namespace Variant0
		{
			void __cdecl NewFunction(const char* filename);

			using OverrideType = decltype(NewFunction)*;
			SDR::Hooking::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(const char* filename)
			{
				
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WaveFixupTmpFile",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericHookVariantInit
					(
						{SDR::Hooking::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						value
					);
				}
			)
		);
	}
}
