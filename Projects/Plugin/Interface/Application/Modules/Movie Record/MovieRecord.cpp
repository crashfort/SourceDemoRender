#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include <ppltasks.h>

#include "Various\Stream.hpp"
#include "Various\Profile.hpp"
#include "Interface\Application\Modules\Shared\EngineClient.hpp"
#include "Interface\Application\Modules\Shared\MaterialSystem.hpp"
#include "Interface\Application\Modules\Shared\SourceGlobals.hpp"
#include "Interface\PluginInterface.hpp"

namespace
{
	namespace Variables
	{
		SDR::Console::Variable OutputDirectory;
		SDR::Console::Variable FlashWindow;
		SDR::Console::Variable ExitOnFinish;
		SDR::Console::Variable SuppressLog;

		namespace Video
		{
			SDR::Console::Variable Framerate;
			SDR::Console::Variable ColorSpace;
			SDR::Console::Variable Encoder;
			SDR::Console::Variable PixelFormat;

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
		SDR::PluginStartupFunctionAdder A1("MovieRecord console variables", []()
		{
			OutputDirectory = SDR::Console::MakeString("sdr_outputdir", "");
			FlashWindow = SDR::Console::MakeBool("sdr_endmovieflash", "0");
			ExitOnFinish = SDR::Console::MakeBool("sdr_endmoviequit", "0");
			SuppressLog = SDR::Console::MakeBool("sdr_movie_suppresslog", "1");

			Video::Framerate = SDR::Console::MakeNumber("sdr_render_framerate", "60", 30, 1000);
			Video::ColorSpace = SDR::Console::MakeString("sdr_movie_encoder_colorspace", "709");
			Video::Encoder = SDR::Console::MakeString("sdr_movie_encoder", "libx264");
			Video::PixelFormat = SDR::Console::MakeString("sdr_movie_encoder_pxformat", "");

			Video::Sample::Multiply = SDR::Console::MakeNumber("sdr_sample_mult", "32", 0);
			Video::Sample::Exposure = SDR::Console::MakeNumber("sdr_sample_exposure", "0.5", 0, 1);

			Video::D3D11::Staging = SDR::Console::MakeBool("sdr_d3d11_staging", "1");

			Video::X264::CRF = SDR::Console::MakeNumberWithString("sdr_x264_crf", "0", 0, 51);
			Video::X264::Preset = SDR::Console::MakeString("sdr_x264_preset", "ultrafast");
			Video::X264::Intra = SDR::Console::MakeBool("sdr_x264_intra", "1");
		});
	}
}

namespace
{	
	void LAVLogFunction(void* avcl, int level, const char* fmt, va_list vl)
	{
		if (!Variables::SuppressLog.GetBool())
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
	struct MovieData
	{
		bool IsStarted = false;

		int OldMatQueueModeValue;

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
				SDR::Log::Warning("SDR: Could not retrieve process memory info\n"s);
				return true;
			}

			return desc.WorkingSetSize > INT32_MAX;
		}

		struct
		{
			bool Enabled;
			float Exposure;

			double TimePerSample;
			double TimePerFrame;
		} SamplingData;

		SDR::Stream::SharedData VideoStreamShared;
		std::unique_ptr<SDR::Stream::StreamBase> VideoStream;

		std::thread FrameBufferThreadHandle;
		std::unique_ptr<SDR::Stream::QueueType> VideoQueue;
	};

	MovieData CurrentMovie;
	std::atomic_int32_t BufferedFrames;
	std::atomic_bool ShouldStopFrameThread;
	std::atomic_bool IsStoppingAsync;

	bool ShouldRecord()
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

		return true;
	}

	void FrameBufferThread()
	{
		auto& movie = CurrentMovie;

		SDR::Stream::FutureData item;

		while (!ShouldStopFrameThread)
		{
			while (movie.VideoQueue->try_dequeue(item))
			{
				--BufferedFrames;

				item.Writer->SetFrameInput(item.Planes);
				item.Writer->SendRawFrame();
			}
		}
	}
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

				/*
					The DX11 texture now contains this data.
				*/
				auto hr = dx9device->StretchRect
				(
					stream->DirectX9.GameRenderTarget0.Get(),
					nullptr,
					stream->DirectX9.SharedSurface.Surface.Get(),
					nullptr,
					D3DTEXF_NONE
				);

				if (FAILED(hr))
				{
					SDR::Log::Warning("SDR: Could not copy D3D9 RT -> D3D11 RT\n"s);
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

					stream->DirectX11.Conversion(CurrentMovie.VideoStreamShared);
					auto res = stream->DirectX11.Download(CurrentMovie.VideoStreamShared, item);

					if (res)
					{
						++BufferedFrames;
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
				auto& movie = CurrentMovie;
				bool dopasses = ShouldRecord();

				if (dopasses)
				{
					/*
						Don't risk running out of memory. Just let the encoding finish so we start fresh with no buffered frames.
					*/
					if (MovieData::WouldNewFrameOverflow())
					{
						while (BufferedFrames)
						{
							std::this_thread::sleep_for(1ms);
						}
					}

					if (movie.VideoStream->FirstFrame)
					{
						movie.VideoStream->FirstFrame = false;
						CopyDX9ToDX11(movie.VideoStream.get());
					}

					else
					{
						Pass(movie.VideoStream.get());
					}
				}
			}
		}

		namespace Variant0
		{
			void __fastcall NewFunction(void* thisptr, void* edx, void* rect);

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __fastcall NewFunction(void* thisptr, void* edx, void* rect)
			{
				ThisHook.GetOriginal()(thisptr, edx, rect);
				Common::Procedure();
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"View_Render",
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
						value
					);
				}
			)
		);
	}

	namespace ModuleStartMovie
	{
		namespace Common
		{
			void VerifyOutputDirectory(const char* path)
			{
				char final[SDR::File::NameSize];
				strcpy_s(final, path);

				PathAddBackslashA(final);

				auto winstr = SDR::String::FromUTF8(final);

				auto res = PathFileExistsW(winstr.c_str()) == 1;

				if (!res)
				{
					SDR::Error::MS::ThrowLastError("Could not access wanted output directory");
				}
			}

			std::string BuildVideoStreamName(const char* savepath, const char* filename)
			{
				char finalname[SDR::File::NameSize];

				PathCombineA(finalname, savepath, filename);

				return {finalname};
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
								SDR::Log::Warning("SDR: Slow encoder preset chosen, this might not work very well for realtime\n"s);
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
					SDR::Error::MS::ThrowLastError("Could not create file, name probably invalid");
				}
			}

			void WarnAboutExtension(const char* filename)
			{
				auto wstr = SDR::String::FromUTF8(filename);				
				auto ext = PathFindExtensionW(wstr.c_str());

				auto containers =
				{
					".avi",
					".mp4",
					".mov",
					".mkv"
				};

				auto showcontainers = [&]()
				{
					for (auto type : containers)
					{
						SDR::Log::Message("SDR: %s\n", type);
					}
				};

				if (*ext == 0)
				{
					SDR::Log::Warning("SDR: No file extension. Available video containers:\n"s);

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
						SDR::Log::Warning("SDR: Unknown file extension. Available video containers:\n"s);

						showcontainers();

						SDR::Error::Make("Unknown file extension"s);
					}
				}
			}

			std::string TrimFilename(const char* filename)
			{
				std::string newname = filename;

				if (std::isspace(newname.back()))
				{
					for (auto it = newname.rbegin(); it != newname.rend(); ++it)
					{
						if (std::isspace(*it))
						{
							newname.pop_back();
						}

						else
						{
							break;
						}
					}
				}

				if (std::isspace(newname.front()))
				{
					newname = newname.substr(newname.find_first_not_of(' ', 0));
				}

				return newname;
			}

			void Procedure(const char* filename, int width, int height)
			{
				auto newname = TrimFilename(filename);
				filename = newname.c_str();

				CurrentMovie = {};

				auto& movie = CurrentMovie;

				try
				{
					WarnAboutExtension(filename);
					WarnAboutName(filename);
					WarnAboutVariableValues();

					auto sdrpath = Variables::OutputDirectory.GetString();

					/*
						No desired path, use game root.
					*/
					if (strlen(sdrpath) == 0)
					{
						sdrpath = SDR::Plugin::GetGamePath();
					}

					else
					{
						VerifyOutputDirectory(sdrpath);
					}

					auto linktabletovariable = [](const char* key, const auto& table, auto& variable)
					{
						for (const auto& entry : table)
						{
							if (_strcmpi(key, entry.first) == 0)
							{
								variable = entry.second;
								return true;
							}
						}

						return false;
					};

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

						linktabletovariable(Variables::Video::ColorSpace.GetString(), table, colorspace);
					}

					if (Variables::SuppressLog.GetBool())
					{
						av_log_set_level(AV_LOG_QUIET);
						av_log_set_callback(nullptr);
					}

					else
					{
						av_log_set_level(AV_LOG_INFO);
						av_log_set_callback(LAVLogFunction);
					}

					auto stream = std::make_unique<SDR::Stream::StreamBase>();

					struct VideoConfigurationData
					{
						using FormatsType = std::vector<std::pair<const char*, AVPixelFormat>>;

						VideoConfigurationData(const char* name, FormatsType&& formats) :
							Encoder(avcodec_find_encoder_by_name(name)),
							PixelFormats(std::move(formats))
						{

						}

						AVCodec* Encoder;
						FormatsType PixelFormats;
					};

					const auto yuv420 = std::make_pair("yuv420", AV_PIX_FMT_YUV420P);
					const auto yuv444 = std::make_pair("yuv444", AV_PIX_FMT_YUV444P);
					const auto bgr0 = std::make_pair("bgr0", AV_PIX_FMT_BGR0);

					VideoConfigurationData table[] =
					{
						VideoConfigurationData("libx264", { yuv420, yuv444 }),
						VideoConfigurationData("libx264rgb", { bgr0 }),
					};

					const VideoConfigurationData* vidconfig = nullptr;

					{
						auto encoderstr = Variables::Video::Encoder.GetString();
						auto encoder = avcodec_find_encoder_by_name(encoderstr);

						SDR::Error::ThrowIfNull(encoder, "Video encoder \"%s\" not found", encoderstr);

						for (const auto& config : table)
						{
							if (config.Encoder == encoder)
							{
								vidconfig = &config;
								break;
							}
						}

						auto pxformatstr = Variables::Video::PixelFormat.GetString();

						if (!linktabletovariable(pxformatstr, vidconfig->PixelFormats, pxformat))
						{
							/*
								User selected pixel format does not match any in config.
							*/
							pxformat = vidconfig->PixelFormats[0].second;
						}

						auto isrgbtype = [](AVPixelFormat format)
						{
							auto table =
							{
								AV_PIX_FMT_BGR0,
							};

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

						stream->Video.Frame.Assign(width, height, pxformat, colorspace, colorrange);

						movie.VideoStreamShared.DirectX11.Create(width, height, MovieData::UseSampling());

						stream->DirectX9.Create(SDR::SourceGlobals::GetD3D9DeviceEx(), width, height);

						stream->DirectX11.Create
						(
							movie.VideoStreamShared.DirectX11.Device.Get(),
							stream->DirectX9.SharedSurface.SharedHandle,
							stream->Video.Frame.Get(),
							MovieData::UseStaging()
						);

						/*
							Destroy any deferred D3D11 resources created by above functions.
						*/
						movie.VideoStreamShared.DirectX11.Context->Flush();

						auto name = BuildVideoStreamName(sdrpath, filename);

						stream->Video.OpenFileForWrite(name.c_str());
						stream->Video.SetEncoder(vidconfig->Encoder);
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
						stream->Video.OpenEncoder(fps, options.Get());

						stream->Video.WriteHeader();
					}

					/*
						All went well, move state over.
					*/
					movie.VideoStream = std::move(stream);
				}

				catch (const SDR::Error::Exception& error)
				{
					CurrentMovie = {};
					return;
				}

				/*
					Don't call the original CL_StartMovie as it causes major recording slowdowns.
				*/

				auto fps = Variables::Video::Framerate.GetInt();
				auto exposure = Variables::Video::Sample::Exposure.GetFloat();
				auto mult = Variables::Video::Sample::Multiply.GetInt();

				auto enginerate = fps;

				movie.SamplingData.Enabled = MovieData::UseSampling();

				if (movie.SamplingData.Enabled)
				{
					enginerate *= mult;

					movie.SamplingData.Exposure = exposure;
					movie.SamplingData.TimePerSample = 1.0 / enginerate;
					movie.SamplingData.TimePerFrame = 1.0 / fps;
				}

				SDR::Console::Variable hostframerate("host_framerate");
				hostframerate.SetValue(enginerate);

				SDR::Console::Variable matqueuemode("mat_queue_mode");
				movie.OldMatQueueModeValue = matqueuemode.GetInt();

				/*
					Force single threaded processing or else there will be flickering.
				*/
				matqueuemode.SetValue(0);

				/*
					Make room for some entries in the queues.
				*/
				movie.VideoQueue = std::make_unique<SDR::Stream::QueueType>(256);

				movie.IsStarted = true;
				BufferedFrames = 0;
				ShouldStopFrameThread = false;
				IsStoppingAsync = false;

				movie.FrameBufferThreadHandle = std::thread(FrameBufferThread);

				SDR::Log::Message("SDR: Started movie\n"s);
			}
		}

		namespace Variant0
		{
			/*
				The 7th parameter (unk) was been added in Source 2013, it's not there in Source 2007.
			*/
			void __cdecl NewFunction(const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk);

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(const char* filename, int flags, int width, int height, float framerate, int jpegquality, int unk)
			{
				Common::Procedure(filename, width, height);
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"StartMovie",
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
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
					SDR::Log::Message("SDR: Movie is already started\n"s);
					return;
				}

				if (args.Count() < 2)
				{
					SDR::Log::Message("SDR: Name is required for startmovie, see Github page for help\n"s);
					return;
				}

				int width;
				int height;
				SDR::MaterialSystem::GetBackBufferDimensions(width, height);

				SDR::Profile::Reset();

				/*
					Retrieve everything after the initial "startmovie" token, in case
					of special UTF8 names the ArgV is split.
				*/
				auto name = args.FullArgs();

				while (true)
				{
					++name;

					if (*name == ' ')
					{
						++name;
						break;
					}
				}

				ModuleStartMovie::Common::Procedure(name, width, height);
			}
		}

		namespace Variant0
		{
			void __cdecl NewFunction(const void* ptr);

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

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
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
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
			void Procedure(bool async)
			{
				if (!CurrentMovie.IsStarted)
				{
					SDR::Log::Message("SDR: No movie is started\n"s);
					return;
				}

				CurrentMovie.IsStarted = false;

				/*
					Don't call original function as we don't call the engine's startmovie.
				*/

				SDR::Console::Variable hostframerate("host_framerate");
				hostframerate.SetValue(0);

				SDR::Console::Variable matqueuemode("mat_queue_mode");
				matqueuemode.SetValue(CurrentMovie.OldMatQueueModeValue);

				SDR::Log::Message("SDR: Ending movie\n"s);

				if (BufferedFrames > 0)
				{
					SDR::Log::Message("SDR: There are %d buffered frames remaining\n", BufferedFrames);
				}

				auto func = []()
				{
					if (!ShouldStopFrameThread)
					{
						ShouldStopFrameThread = true;
						CurrentMovie.FrameBufferThreadHandle.join();
					}

					/*
						Let the encoder finish all the delayed frames.
					*/
					CurrentMovie.VideoStream->Video.Finish();

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

					SDR::Log::MessageColor({ 88, 255, 39 }, "SDR: Movie is now complete\n"s);

					SDR::Profile::ShowResults();
				};

				if (async)
				{
					IsStoppingAsync = true;

					auto task = concurrency::create_task(func);

					task.then([]()
					{
						IsStoppingAsync = false;
					});
				}

				else
				{
					func();
				}
			}
		}

		namespace Variant0
		{
			void __cdecl NewFunction();

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction()
			{
				Common::Procedure(true);
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"EndMovie",
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
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
				ModuleEndMovie::Common::Procedure(true);
			}
		}

		namespace Variant0
		{
			void __cdecl NewFunction();

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

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
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
						value
					);
				}
			)
		);
	}

	/*
		This function handles the event when endmovie wasn't called on quit.
		The cleaning up cannot be done asynchronously as the module itself gets unloaded.
	*/
	SDR::PluginShutdownFunctionAdder A1([]()
	{
		if (IsStoppingAsync)
		{
			SDR::Log::Message("SDR: Already stopping asynchronously\n"s);

			while (IsStoppingAsync)
			{
				std::this_thread::sleep_for(1ms);
			}
		}

		ModuleEndMovie::Common::Procedure(false);
	});

	namespace ModuleSUpdateGuts
	{
		namespace Variant0
		{
			void __cdecl NewFunction(float mixahead);

			using OverrideType = decltype(NewFunction)*;
			SDR::HookModule<OverrideType> ThisHook;

			void __cdecl NewFunction(float mixahead)
			{
				if (!CurrentMovie.IsStarted)
				{
					ThisHook.GetOriginal()(mixahead);
				}
			}
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"SUpdateGuts",
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericHookVariantInit
					(
						{SDR::GenericHookInitParam(Variant0::ThisHook, Variant0::NewFunction)},
						name,
						value
					);
				}
			)
		);
	}
}
