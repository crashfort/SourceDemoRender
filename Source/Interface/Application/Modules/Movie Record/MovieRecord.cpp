#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include "dbg.h"
#include "view_shared.h"

extern "C"
{
	#include "libavutil\avutil.h"
	#include "libavutil\imgutils.h"
	#include "libavcodec\avcodec.h"
	#include "libavformat\avformat.h"
	#include "libswscale\swscale.h"
}

#include <ppltasks.h>

/*
	For WAVE related things
*/
#include <mmsystem.h>

#include <d3d9.h>
#include <d3d11.h>

#include "readerwriterqueue.h"

namespace
{
	namespace LAV
	{
		enum class ExceptionType
		{
			AllocSWSContext,
			AllocAVFrame,
			AllocVideoStream,
			VideoEncoderNotFound,
			OpenWaveFile,
		};

		const char* ExceptionTypeToString(ExceptionType code)
		{
			auto index = static_cast<int>(code);

			static const char* names[] =
			{
				"Could not allocate video conversion context",
				"Could not allocate video frame",
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

		inline void ThrowIfNull(const void* ptr, ExceptionType code)
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

				ThrowIfNull
				(
					Context,
					ExceptionType::AllocSWSContext
				);
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
					avformat_alloc_output_context2
					(
						&Context,
						nullptr,
						nullptr,
						filename
					)
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

		struct ScopedAVFrame
		{
			~ScopedAVFrame()
			{
				if (Frame)
				{
					av_frame_free(&Frame);
				}
			}

			void Assign
			(
				int width,
				int height,
				AVPixelFormat format,
				AVColorSpace colorspace,
				AVColorRange colorrange
			)
			{
				Frame = av_frame_alloc();

				ThrowIfNull(Frame, ExceptionType::AllocAVFrame);

				Frame->format = format;
				Frame->width = width;
				Frame->height = height;
				Frame->colorspace = colorspace;
				Frame->color_range = colorrange;

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

		namespace Variables
		{
			ConVar SuppressLog
			(
				"sdr_movie_suppresslog", "0", FCVAR_NEVER_AS_STRING,
				"Disable logging output from LAV",
				true, 0, true, 1
			);
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
}

namespace
{
	namespace Profile
	{
		const char* Names[] =
		{
			"ViewRender",
			"DX11Proc",
			"PushYUV",
			"PushRGB",
			"Encode",
			"WriteEncodedPacket",
		};

		namespace Types
		{
			enum Type
			{
				ViewRender,
				DX11Proc,
				PushYUV,
				PushRGB,
				Encode,
				WriteEncodedPacket,

				Count
			};
		}

		auto GetTimeNow()
		{
			return std::chrono::high_resolution_clock::now();
		}

		using TimePointType = decltype(GetTimeNow());

		std::chrono::nanoseconds GetTimeDifference(TimePointType start)
		{
			using namespace std::chrono;

			auto now = GetTimeNow();
			auto difference = now - start;

			auto time = duration_cast<nanoseconds>(difference);

			return time;
		}

		struct Entry
		{
			uint32_t Calls = 0;
			std::chrono::nanoseconds TotalTime = 0ns;
		};

		std::array<Entry, Types::Count> Entries;

		struct ScopedEntry
		{
			ScopedEntry(Types::Type entry) : 
				Target(Entries[entry]),
				Start(GetTimeNow())
			{
				++Target.Calls;
			}

			~ScopedEntry()
			{
				Target.TotalTime += GetTimeDifference(Start);
			}

			TimePointType Start;
			Entry& Target;
		};
	}

	namespace Variables
	{
		ConVar FrameRate
		(
			"sdr_render_framerate", "60", FCVAR_NEVER_AS_STRING,
			"Movie output framerate",
			true, 30, true, 1000
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

		namespace Pass
		{
			ConVar Fullbright
			(
				"sdr_pass_fullbright", "0", FCVAR_NEVER_AS_STRING,
				"Perform extra fullbright pass to separate video",
				true, 0, true, 1
			);
		}

		namespace Video
		{
			ConVar Encoder
			{
				"sdr_movie_encoder", "libx264", 0,
				"Video encoder"
				"Values: libx264, libx264rgb",
				[](IConVar* var, const char* oldstr, float oldfloat)
				{
					auto newstr = Encoder.GetString();

					auto encoder = avcodec_find_encoder_by_name(newstr);

					if (!encoder)
					{
						Warning
						(
							"SDR: Encoder %s not found\n",
							newstr
						);

						Msg("SDR: Available encoders: \n");

						auto next = av_codec_next(nullptr);

						while (next)
						{
							Msg("SDR: * %s\n", next->name);
							
							next = av_codec_next(next);
						}
					}
				}
			};

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
					"sdr_x264_crf", "0", 0,
					"Constant rate factor value. Values: 0 (best) - 51 (worst). "
					"See https://trac.ffmpeg.org/wiki/Encode/H.264",
					true, 0, true, 51
				);

				ConVar Preset
				{
					"sdr_x264_preset", "ultrafast", 0,
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

				ConVar Intra
				(
					"sdr_x264_intra", "1", 0,
					"Whether to produce a video of only keyframes",
					true, 0, true, 1
				);
			}
		}
	}

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
			try
			{
				WaveFile.Assign(name, "wb");
			}

			catch (SDR::Shared::ScopedFile::ExceptionType status)
			{
				LAV::ThrowIfNull
				(
					nullptr,
					LAV::ExceptionType::OpenWaveFile
				);
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

			WaveFile.WriteRegion(buffer, length);

			DataLength += length;
			FileLength += DataLength;
		}

		SDR::Shared::ScopedFile WaveFile;
		
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
		enum
		{
			PlaneCount = 3
		};

		using PlaneType = std::array<std::vector<uint8_t>, PlaneCount>;

		void OpenFileForWrite(const char* path)
		{
			FormatContext.Assign(path);

			if ((FormatContext->oformat->flags & AVFMT_NOFILE) == 0)
			{
				LAV::ThrowIfFailed
				(
					avio_open
					(
						&FormatContext->pb,
						path,
						AVIO_FLAG_WRITE
					)
				);
			}
		}

		void SetEncoder(AVCodec* encoder)
		{
			Encoder = encoder;
			LAV::ThrowIfNull(Encoder, LAV::ExceptionType::VideoEncoderNotFound);

			Stream = avformat_new_stream(FormatContext.Get(), Encoder);
			LAV::ThrowIfNull(Stream, LAV::ExceptionType::AllocVideoStream);

			CodecContext = Stream->codec;
		}

		void OpenEncoder(int framerate, AVDictionary** options)
		{
			CodecContext->width = Frame->width;
			CodecContext->height = Frame->height;
			CodecContext->pix_fmt = (AVPixelFormat)Frame->format;
			CodecContext->colorspace = Frame->colorspace;
			CodecContext->color_range = Frame->color_range;

			if (FormatContext->oformat->flags & AVFMT_GLOBALHEADER)
			{
				CodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}

			AVRational timebase;
			timebase.num = 1;
			timebase.den = framerate;

			auto inversetime = av_inv_q(timebase);

			CodecContext->time_base = timebase;
			CodecContext->framerate = inversetime;

			LAV::ThrowIfFailed
			(
				avcodec_open2
				(
					CodecContext,
					Encoder,
					options
				)
			);

			LAV::ThrowIfFailed
			(
				avcodec_parameters_from_context
				(
					Stream->codecpar,
					CodecContext
				)
			);

			Stream->time_base = timebase;
			Stream->avg_frame_rate = inversetime;
		}

		void WriteHeader()
		{
			LAV::ThrowIfFailed
			(
				avformat_write_header
				(
					FormatContext.Get(),
					nullptr
				)
			);
		}

		void WriteTrailer()
		{
			LAV::ThrowIfFailed
			(
				av_write_trailer
				(
					FormatContext.Get()
				)
			);
		}

		void SetFrameInput(PlaneType& planes)
		{
			int index = 0;

			for (auto& plane : planes)
			{
				if (plane.empty())
				{
					break;
				}

				Frame->data[index] = plane.data();
				++index;
			}
		}

		void SendRawFrame()
		{
			{
				Profile::ScopedEntry e1(Profile::Types::Encode);

				Frame->pts = PresentationIndex;
				PresentationIndex++;

				auto ret = avcodec_send_frame
				(
					CodecContext,
					Frame.Get()
				);
			}

			ReceivePacketFrame();
		}

		void SendFlushFrame()
		{
			auto ret = avcodec_send_frame
			(
				CodecContext,
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

			AVPacket packet = {};
			av_init_packet(&packet);

			while (status == 0)
			{
				status = avcodec_receive_packet
				(
					CodecContext,
					&packet
				);

				if (status < 0)
				{
					return;
				}

				WriteEncodedPacket(packet);
			}
		}

		void WriteEncodedPacket(AVPacket& packet)
		{
			Profile::ScopedEntry e1(Profile::Types::WriteEncodedPacket);

			av_packet_rescale_ts
			(
				&packet,
				CodecContext->time_base,
				Stream->time_base
			);

			packet.stream_index = Stream->index;

			av_interleaved_write_frame
			(
				FormatContext.Get(),
				&packet
			);
		}

		LAV::ScopedFormatContext FormatContext;
		
		/*
			This gets freed when FormatContext gets destroyed
		*/
		AVCodecContext* CodecContext;
		AVCodec* Encoder = nullptr;
		AVStream* Stream = nullptr;
		LAV::ScopedAVFrame Frame;

		/*
			Incremented and written to for every sent frame
		*/
		int64_t PresentationIndex = 0;
	};

	namespace ModuleMaterialSystem
	{
		namespace Types
		{
			using GetBackBufferDimensions = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				int& width,
				int& height
			);

			using BeginFrame = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				float frametime
			);

			using EndFrame = void(__fastcall*)
			(
				void* thisptr,
				void* edx
			);
		}
		
		void* MaterialsPtr;
		Types::GetBackBufferDimensions GetBackBufferDimensions;
		Types::BeginFrame BeginFrame;
		Types::EndFrame EndFrame;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_MaterialsPtr",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					MaterialsPtr = **(void***)(address);

					SDR::ModuleShared::Registry::SetKeyValue
					(
						name,
						MaterialsPtr
					);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_GetBackBufferDimensions",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonFlex(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetBackBufferDimensions,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_BeginFrame",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonFlex(value);

					return SDR::ModuleShared::SetFromAddress
					(
						BeginFrame,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_EndFrame",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonFlex(value);

					return SDR::ModuleShared::SetFromAddress
					(
						EndFrame,
						address
					);
				}
			)
		);
	}

	namespace ModuleView
	{
		namespace Types
		{
			using RenderView = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				const void* view,
				int clearflags,
				int whattodraw
			);

			using GetViewSetup = const void*(__fastcall*)
			(
				void* thisptr,
				void* edx
			);
		}
		
		void* ViewPtr;
		Types::RenderView RenderView;
		Types::GetViewSetup GetViewSetup;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"View_ViewPtr",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					ViewPtr = **(void***)(address);

					SDR::ModuleShared::Registry::SetKeyValue
					(
						name,
						ViewPtr
					);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"View_RenderView",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonFlex(value);

					return SDR::ModuleShared::SetFromAddress
					(
						RenderView,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"View_GetViewSetup",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonFlex(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetViewSetup,
						address
					);
				}
			)
		);
	}

	namespace ModuleSourceGlobals
	{
		IDirect3DDevice9* DX9Device;
		bool* DrawLoading;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"D3D9_Device",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					DX9Device = **(IDirect3DDevice9***)(address);

					SDR::ModuleShared::Registry::SetKeyValue
					(
						name,
						DX9Device
					);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"DrawLoading",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					DrawLoading = *(bool**)(address);

					SDR::ModuleShared::Registry::SetKeyValue
					(
						name,
						DrawLoading
					);

					return true;
				}
			)
		);
	}

	struct MovieData
	{
		bool IsStarted = false;

		uint32_t Width;
		uint32_t Height;

		struct VideoStreamBase;

		struct VideoFutureData
		{
			VideoStreamBase* Stream;
			SDRVideoWriter::PlaneType Planes;
		};

		using VideoQueueType = moodycamel::ReaderWriterQueue<VideoFutureData>;

		struct VideoStreamBase
		{
			struct DirectX9Data
			{
				struct SharedSurfaceData
				{
					void Create(IDirect3DDevice9* device, int width, int height)
					{
						MS::ThrowIfFailed
						(
							device->CreateOffscreenPlainSurface
							(
								width,
								height,
								D3DFMT_A8R8G8B8,
								D3DPOOL_DEFAULT,
								Surface.GetAddressOf(),
								&SharedHandle
							)
						);
					}

					HANDLE SharedHandle = nullptr;
					Microsoft::WRL::ComPtr<IDirect3DSurface9> Surface;
				};

				SharedSurfaceData SharedSurface;
			} DirectX9;

			struct DirectX11Data
			{
				struct FrameBufferBase
				{
					virtual ~FrameBufferBase() = default;

					virtual void Create(ID3D11Device* device, AVFrame* reference) = 0;

					/*
						States that need update every frame
					*/
					virtual void DynamicBind(ID3D11DeviceContext* context) = 0;

					/*
						Try to retrieve data to CPU after an operation
					*/
					virtual bool Download(ID3D11DeviceContext* context, MovieData::VideoFutureData& item) = 0;
				};

				struct GPUBuffer
				{
					void Create(ID3D11Device* device, DXGI_FORMAT viewformat, int size, int numelements)
					{
						D3D11_BUFFER_DESC desc = {};
						desc.ByteWidth = size;
						desc.Usage = D3D11_USAGE_DEFAULT;
						desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
						desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

						MS::ThrowIfFailed
						(
							device->CreateBuffer
							(
								&desc,
								nullptr,
								Buffer.GetAddressOf()
							)
						);

						D3D11_UNORDERED_ACCESS_VIEW_DESC viewdesc = {};
						viewdesc.Format = viewformat;
						viewdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
						viewdesc.Buffer.NumElements = numelements;

						MS::ThrowIfFailed
						(
							device->CreateUnorderedAccessView
							(
								Buffer.Get(),
								&viewdesc,
								View.GetAddressOf()
							)
						);
					}

					HRESULT Map(ID3D11DeviceContext* context, D3D11_MAPPED_SUBRESOURCE* mapped)
					{
						return context->Map(Buffer.Get(), 0, D3D11_MAP_READ, 0, mapped);
					}

					void Unmap(ID3D11DeviceContext* context)
					{
						context->Unmap(Buffer.Get(), 0);
					}

					Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
					Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> View;
				};

				struct RGBAFrameBuffer : FrameBufferBase
				{
					virtual void Create(ID3D11Device* device, AVFrame* reference) override
					{
						auto size = reference->buf[0]->size;
						auto count = size / sizeof(uint32_t);

						Buffer.Create(device, DXGI_FORMAT_R32_UINT, size, count);
					}

					virtual void DynamicBind(ID3D11DeviceContext* context) override
					{
						auto uavs =
						{
							Buffer.View.Get(),
						};

						context->CSSetUnorderedAccessViews(0, 1, uavs.begin(), nullptr);
					}

					virtual bool Download(ID3D11DeviceContext* context, MovieData::VideoFutureData& item) override
					{
						Profile::ScopedEntry e1(Profile::Types::PushRGB);

						D3D11_MAPPED_SUBRESOURCE mapped;

						auto hr = Buffer.Map(context, &mapped);

						if (FAILED(hr))
						{
							Warning("SDR: Could not map DX11 RGB buffer\n");
						}

						else
						{
							auto ptr = (uint8_t*)mapped.pData;
							item.Planes[0].assign(ptr, ptr + mapped.RowPitch);
						}

						Buffer.Unmap(context);

						return SUCCEEDED(hr);
					}

					GPUBuffer Buffer;
				};

				struct YUVFrameBuffer : FrameBufferBase
				{
					virtual void Create(ID3D11Device* device, AVFrame* reference) override
					{
						auto sizey = reference->buf[0]->size;
						auto sizeu = reference->buf[1]->size;
						auto sizev = reference->buf[2]->size;

						Y.Create(device, DXGI_FORMAT_R8_UINT, sizey, sizey);
						U.Create(device, DXGI_FORMAT_R8_UINT, sizeu, sizeu);
						V.Create(device, DXGI_FORMAT_R8_UINT, sizev, sizev);

						__declspec(align(16)) struct
						{
							int Strides[3];
						} constantbufferdata;

						constantbufferdata.Strides[0] = reference->linesize[0];
						constantbufferdata.Strides[1] = reference->linesize[1];
						constantbufferdata.Strides[2] = reference->linesize[2];

						D3D11_BUFFER_DESC cbufdesc = {};
						cbufdesc.ByteWidth = sizeof(constantbufferdata);
						cbufdesc.Usage = D3D11_USAGE_DEFAULT;
						cbufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

						D3D11_SUBRESOURCE_DATA cbufsubdesc = {};
						cbufsubdesc.pSysMem = &constantbufferdata;

						MS::ThrowIfFailed
						(
							device->CreateBuffer
							(
								&cbufdesc,
								&cbufsubdesc,
								ConstantBuffer.GetAddressOf()
							)
						);
					}

					virtual void DynamicBind(ID3D11DeviceContext* context) override
					{
						auto cbufs =
						{
							ConstantBuffer.Get()
						};

						context->CSSetConstantBuffers(0, 1, cbufs.begin());

						auto uavs =
						{
							Y.View.Get(),
							U.View.Get(),
							V.View.Get(),
						};

						context->CSSetUnorderedAccessViews(0, 3, uavs.begin(), nullptr);
					}

					virtual bool Download(ID3D11DeviceContext* context, MovieData::VideoFutureData& item) override
					{
						Profile::ScopedEntry e1(Profile::Types::PushYUV);

						D3D11_MAPPED_SUBRESOURCE mappedy;
						D3D11_MAPPED_SUBRESOURCE mappedu;
						D3D11_MAPPED_SUBRESOURCE mappedv;

						auto hrs =
						{
							Y.Map(context, &mappedy),
							U.Map(context, &mappedu),
							V.Map(context, &mappedv)
						};

						bool pass = true;

						for (auto res : hrs)
						{
							if (FAILED(res))
							{
								pass = false;

								Warning("SDR: Could not map DX11 YUV buffers\n");
								break;
							}
						}

						if (pass)
						{
							auto ptry = (uint8_t*)mappedy.pData;
							auto ptru = (uint8_t*)mappedu.pData;
							auto ptrv = (uint8_t*)mappedv.pData;

							item.Planes[0].assign(ptry, ptry + mappedy.RowPitch);
							item.Planes[1].assign(ptru, ptru + mappedu.RowPitch);
							item.Planes[2].assign(ptrv, ptrv + mappedv.RowPitch);
						}

						Y.Unmap(context);
						U.Unmap(context);
						V.Unmap(context);

						return pass;
					}

					GPUBuffer Y;
					GPUBuffer U;
					GPUBuffer V;

					Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
				};

				void Create
				(
					ID3D11Device* device,
					HANDLE dx9handle,
					AVFrame* reference
				)
				{
					Microsoft::WRL::ComPtr<ID3D11Resource> tempresource;

					MS::ThrowIfFailed
					(
						device->OpenSharedResource
						(
							dx9handle,
							IID_PPV_ARGS(tempresource.GetAddressOf())
						)
					);

					MS::ThrowIfFailed
					(
						tempresource.As(&SharedTexture)
					);

					MS::ThrowIfFailed
					(
						device->CreateShaderResourceView
						(
							SharedTexture.Get(),
							nullptr,
							SharedTextureSRV.GetAddressOf()
						)
					);

					auto openshader = []
					(
						ID3D11Device* device,
						const char* name,
						ID3D11ComputeShader** shader
					)
					{
						using Status = SDR::Shared::ScopedFile::ExceptionType;

						SDR::Shared::ScopedFile file;

						char path[1024];
						strcpy_s(path, SDR::GetGamePath());
						strcat(path, R"(SDR\)");
						strcat_s(path, name);
						strcat_s(path, ".sdrshader");

						try
						{
							file.Assign(path, "rb");
						}

						catch (Status status)
						{
							if (status == Status::CouldNotOpenFile)
							{
								Warning
								(
									"SDR: Could not open shader \"%s\"\n",
									path
								);
							}

							throw false;
						}

						auto data = file.ReadAll();

						MS::ThrowIfFailed
						(
							device->CreateComputeShader
							(
								data.data(),
								data.size(),
								nullptr,
								shader
							)
						);
					};

					struct ConversionRuleData
					{
						ConversionRuleData
						(
							AVPixelFormat format,
							const char* shader,
							bool yuv
						) :
							Format(format),
							ShaderName(shader),
							IsYUV(yuv)
						{

						}

						AVPixelFormat Format;
						const char* ShaderName;
						bool IsYUV;
					};

					ConversionRuleData table[] =
					{
						ConversionRuleData(AV_PIX_FMT_YUV420P, "RGB32_To_YUV420", true),
						ConversionRuleData(AV_PIX_FMT_YUV444P, "RGB32_To_YUV444", true),

						/*
							libx264rgb
						*/
						ConversionRuleData(AV_PIX_FMT_BGR0, "BGRA_PackRemoveAlpha", false),
					};

					ConversionRuleData* found = nullptr;

					for (auto&& entry : table)
					{
						if (entry.Format == reference->format)
						{
							openshader
							(
								device,
								entry.ShaderName,
								ComputeShader.GetAddressOf()
							);

							found = &entry;
							break;
						}
					}

					if (!found)
					{
						Warning
						(
							"SDR: No conversion rule found for %s\n",
							av_get_pix_fmt_name((AVPixelFormat)reference->format)
						);

						throw false;
					}

					if (found->IsYUV)
					{
						GPUFrameBuffer = std::make_unique<YUVFrameBuffer>();
					}

					else
					{
						GPUFrameBuffer = std::make_unique<RGBAFrameBuffer>();
					}

					GPUFrameBuffer->Create(device, reference);
				}

				Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShader;

				Microsoft::WRL::ComPtr<ID3D11Texture2D> SharedTexture;
				Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SharedTextureSRV;

				std::unique_ptr<FrameBufferBase> GPUFrameBuffer;
			} DirectX11;

			SDRVideoWriter Video;

			virtual const char* GetSuffix() const
			{
				return nullptr;
			}

			void DynamicBind(ID3D11DeviceContext* context)
			{
				context->CSSetShader(DirectX11.ComputeShader.Get(), nullptr, 0);
				DirectX11.GPUFrameBuffer->DynamicBind(context);
			}

			virtual void PreRender()
			{

			}

			virtual void PostRender()
			{

			}
		};

		struct FullbrightVideoStream : VideoStreamBase
		{
			virtual const char* GetSuffix() const override
			{
				return "fullbright";
			}

			virtual void PreRender()
			{
				ConVarRef ref("mat_fullbright");
				ref.SetValue(1);
			}

			virtual void PostRender()
			{
				ConVarRef ref("mat_fullbright");
				ref.SetValue(0);
			}
		};

		std::unique_ptr<SDRAudioWriter> Audio;
		std::vector<std::unique_ptr<VideoStreamBase>> VideoStreams;

		std::thread FrameBufferThreadHandle;
		std::unique_ptr<VideoQueueType> VideoQueue;

		Microsoft::WRL::ComPtr<ID3D11Device> Device;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
	};

	MovieData CurrentMovie;
	std::atomic_int32_t BufferedFrames;
	std::atomic_bool ShouldStopFrameThread;

	void SDR_MovieShutdown()
	{
		auto& movie = CurrentMovie;

		if (!movie.IsStarted)
		{
			movie = MovieData();
			return;
		}

		if (!ShouldStopFrameThread)
		{
			ShouldStopFrameThread = true;
			movie.FrameBufferThreadHandle.join();
		}

		movie = MovieData();
	}

	/*
		In case endmovie never gets called,
		this handles the plugin_unload
	*/
	SDR::PluginShutdownFunctionAdder A1(SDR_MovieShutdown);

	void FrameBufferThread()
	{
		auto& movie = CurrentMovie;

		MovieData::VideoFutureData item;

		while (!ShouldStopFrameThread)
		{
			while (movie.VideoQueue->try_dequeue(item))
			{
				--BufferedFrames;

				item.Stream->Video.SetFrameInput(item.Planes);
				item.Stream->Video.SendRawFrame();
			}
		}
	}
}

namespace
{	
	namespace ModuleView_Render
	{
		#pragma region Init

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			void* rect
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"View_Render",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		void Pass(MovieData::VideoStreamBase* stream)
		{
			HRESULT hr;
			Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;

			hr = ModuleSourceGlobals::DX9Device->GetRenderTarget
			(
				0,
				surface.GetAddressOf()
			);

			if (FAILED(hr))
			{
				Warning("SDR: Could not get DX9 RT\n");
				return;
			}

			/*
				The DX11 texture now contains this data
			*/
			hr = ModuleSourceGlobals::DX9Device->StretchRect
			(
				surface.Get(),
				nullptr,
				stream->DirectX9.SharedSurface.Surface.Get(),
				nullptr,
				D3DTEXF_NONE
			);

			if (FAILED(hr))
			{
				Warning("SDR: Could not copy DX9 RT -> DX11 RT\n");
				return;
			}

			/*
				Convert RGBA32 to wanted format
			*/
			{
				Profile::ScopedEntry e1(Profile::Types::DX11Proc);

				HRESULT hr;
				auto& context = CurrentMovie.Context;

				auto srvs =
				{
					stream->DirectX11.SharedTextureSRV.Get()
				};

				context->CSSetShaderResources(0, 1, srvs.begin());

				stream->DynamicBind(context.Get());

				context->Dispatch
				(
					std::ceil(CurrentMovie.Width / 8.0),
					std::ceil(CurrentMovie.Height / 8.0),
					1
				);

				MovieData::VideoFutureData item;
				item.Stream = stream;
				
				auto res = stream->DirectX11.GPUFrameBuffer->Download(context.Get(), item);

				if (res)
				{
					++BufferedFrames;
					CurrentMovie.VideoQueue->enqueue(std::move(item));
				}
			}
		}

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			void* rect
		)
		{
			ThisHook.GetOriginal()
			(
				thisptr,
				edx,
				rect
			);

			bool dopasses = true;

			auto& movie = CurrentMovie;
			auto& interfaces = SDR::GetEngineInterfaces();
			auto client = interfaces.EngineClient;

			if (!CurrentMovie.IsStarted)
			{
				dopasses = false;
			}

			if (*ModuleSourceGlobals::DrawLoading)
			{
				dopasses = false;
			}

			if (client->Con_IsVisible())
			{
				dopasses = false;
			}

			if (dopasses)
			{
				Profile::ScopedEntry e1(Profile::Types::ViewRender);

				auto count = movie.VideoStreams.size();

				if (count > 1)
				{
					for (auto& stream : movie.VideoStreams)
					{
						stream->PreRender();

						ModuleMaterialSystem::EndFrame
						(
							ModuleMaterialSystem::MaterialsPtr,
							nullptr
						);

						ModuleMaterialSystem::BeginFrame
						(
							ModuleMaterialSystem::MaterialsPtr,
							nullptr,
							0
						);

						auto setup = ModuleView::GetViewSetup
						(
							ModuleView::ViewPtr,
							nullptr
						);

						ModuleView::RenderView
						(
							ModuleView::ViewPtr,
							nullptr,
							setup,
							0,
							RENDERVIEW_DRAWVIEWMODEL | RENDERVIEW_DRAWHUD
						);

						Pass(stream.get());

						stream->PostRender();
					}
				}

				else if (count == 1)
				{
					auto& stream = movie.VideoStreams[0];
					
					stream->PreRender();
					Pass(stream.get());
					stream->PostRender();
				}
			}
		}
	}

	namespace ModuleStartMovie
	{
		#pragma region Init

		void __cdecl Override
		(
			const char* filename,
			int flags,
			int width,
			int height,
			float framerate,
			int jpegquality,
			int unk
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"StartMovie",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		void CreateOutputDirectory(const char* path)
		{
			char final[1024];
			strcpy_s(final, path);

			V_AppendSlash(final, sizeof(final));

			auto res = SHCreateDirectoryExA(nullptr, final, nullptr);

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
					Warning
					(
						"SDR: Movie output path is invalid\n"
					);

					throw false;
				}

				case ERROR_CANCELLED:
				{
					Warning
					(
						"SDR: Extra directories were created but are hidden, aborting\n"
					);

					throw false;
				}

				default:
				{
					Warning
					(
						"SDR: Some unknown error happened when starting movie, "
						"related to sdr_outputdir\n"
					);

					throw false;
				}
			}
		}

		std::string BuildVideoStreamName
		(
			const char* savepath,
			const char* filename,
			MovieData::VideoStreamBase* stream
		)
		{
			char finalname[2048];
			char finalfilename[1024];
			char extension[64];

			auto suffix = stream->GetSuffix();

			if (suffix)
			{
				auto ptr = V_GetFileExtension(filename);
				strcpy_s(extension, ptr);

				V_StripExtension(filename, finalfilename, sizeof(finalfilename));

				strcat_s(finalfilename, "_");
				strcat_s(finalfilename, suffix);

				strcat_s(finalfilename, ".");
				strcat_s(finalfilename, extension);
			}

			else
			{
				strcpy_s(finalfilename, filename);
			}

			V_ComposeFileName
			(
				savepath,
				finalfilename,
				finalname,
				sizeof(finalname)
			);

			return {finalname};
		}

		std::string BuildAudioName
		(
			const char* savepath,
			const char* filename
		)
		{
			char finalname[2048];
			char finalfilename[1024];

			V_StripExtension(filename, finalfilename, sizeof(finalfilename));
			strcat_s(finalfilename, ".wav");

			V_ComposeFileName
			(
				savepath,
				finalfilename,
				finalname,
				sizeof(finalname)
			);

			return {finalname};
		}

		/*
			The 7th parameter (unk) was been added in Source 2013,
			it's not there in Source 2007
		*/
		void __cdecl Override
		(
			const char* filename,
			int flags,
			int width,
			int height,
			float framerate,
			int jpegquality,
			int unk
		)
		{
			auto& movie = CurrentMovie;

			auto sdrpath = Variables::OutputDirectory.GetString();

			try
			{
				CreateOutputDirectory(sdrpath);
			}

			catch (bool value)
			{
				return;
			}

			auto colorspace = AVCOL_SPC_BT470BG;
			auto colorrange = AVCOL_RANGE_MPEG;
			auto pxformat = AV_PIX_FMT_NONE;
				
			movie.Width = width;
			movie.Height = height;

			try
			{
				av_log_set_callback(LAV::LogFunction);

				std::vector<std::unique_ptr<MovieData::VideoStreamBase>> tempstreams;
				tempstreams.emplace_back(std::make_unique<MovieData::VideoStreamBase>());

				if (Variables::Pass::Fullbright.GetBool())
				{
					tempstreams.emplace_back(std::make_unique<MovieData::FullbrightVideoStream>());
				}

				std::unique_ptr<SDRAudioWriter> tempaudio;

				if (Variables::Audio::Enable.GetBool())
				{
					tempaudio = std::make_unique<SDRAudioWriter>();
				}

				auto audiowriter = tempaudio.get();

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

				struct VideoConfigurationData
				{
					AVCodec* Encoder;
					std::vector<std::pair<const char*, AVPixelFormat>> PixelFormats;
				};

				std::vector<VideoConfigurationData> videoconfigs;
				const VideoConfigurationData* vidconfig = nullptr;

				{
					auto i420 = std::make_pair("i420", AV_PIX_FMT_YUV420P);
					auto i444 = std::make_pair("i444", AV_PIX_FMT_YUV444P);
					auto bgr0 = std::make_pair("bgr0", AV_PIX_FMT_BGR0);

					videoconfigs.emplace_back();
					auto& x264 = videoconfigs.back();
					x264.Encoder = avcodec_find_encoder_by_name("libx264");
					x264.PixelFormats =
					{
						i420,
						i444,
					};

					videoconfigs.emplace_back();
					auto& x264rgb = videoconfigs.back();
					x264rgb.Encoder = avcodec_find_encoder_by_name("libx264rgb");
					x264rgb.PixelFormats =
					{
						bgr0,
					};
				}

				{
					auto encoderstr = Variables::Video::Encoder.GetString();

					auto encoder = avcodec_find_encoder_by_name(encoderstr);
					LAV::ThrowIfNull(encoder, LAV::ExceptionType::VideoEncoderNotFound);

					for (const auto& config : videoconfigs)
					{
						if (config.Encoder == encoder)
						{
							vidconfig = &config;
							break;
						}
					}

					if (!vidconfig)
					{
						Warning
						(
							"SDR: Encoder %s not found\n",
							encoderstr
						);

						LAV::ThrowIfNull(vidconfig, LAV::ExceptionType::VideoEncoderNotFound);
					}

					auto pxformatstr = Variables::Video::PixelFormat.GetString();

					linktabletovariable(pxformatstr, vidconfig->PixelFormats, pxformat);

					/*
						User selected pixel format does not match any in config
					*/
					if (pxformat == AV_PIX_FMT_NONE)
					{
						pxformat = vidconfig->PixelFormats[0].second;
					}

					/*
						Not setting this will leave different colors across
						multiple programs
					*/

					auto isrgbtype = [](AVPixelFormat format)
					{
						auto table =
						{
							AV_PIX_FMT_RGB24,
							AV_PIX_FMT_BGR24,
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
						colorrange = AVCOL_RANGE_UNSPECIFIED;
						colorspace = AVCOL_SPC_RGB;
					}

					for (auto& stream : tempstreams)
					{
						stream->Video.Frame.Assign
						(
							width,
							height,
							pxformat,
							colorspace,
							colorrange
						);
					}

					try
					{
						uint32_t flags = 0;
						#ifdef _DEBUG
						flags |= D3D11_CREATE_DEVICE_DEBUG;
						#endif

						MS::ThrowIfFailed
						(
							D3D11CreateDevice
							(
								nullptr,
								D3D_DRIVER_TYPE_HARDWARE,
								0,
								flags,
								nullptr,
								0,
								D3D11_SDK_VERSION,
								movie.Device.GetAddressOf(),
								0,
								movie.Context.GetAddressOf()
							)
						);

						for (auto& stream : tempstreams)
						{
							stream->DirectX9.SharedSurface.Create
							(
								ModuleSourceGlobals::DX9Device,
								width,
								height
							);

							stream->DirectX11.Create
							(
								movie.Device.Get(),
								stream->DirectX9.SharedSurface.SharedHandle,
								stream->Video.Frame.Get()
							);
						}
					}

					catch (HRESULT status)
					{
						return;
					}

					catch (bool value)
					{
						return;
					}

					for (auto& stream : tempstreams)
					{
						auto name = BuildVideoStreamName
						(
							sdrpath,
							filename,
							stream.get()
						);

						stream->Video.OpenFileForWrite(name.c_str());
						stream->Video.SetEncoder(vidconfig->Encoder);
					}

					if (audiowriter)
					{
						auto name = BuildAudioName(sdrpath, filename);

						/*
							This is the only supported audio output format
						*/
						audiowriter->Open(name.c_str(), 44'100, 16, 2);
					}
				}

				for (auto& stream : tempstreams)
				{
					LAV::ScopedAVDictionary options;

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
								Setting every frame as a keyframe
								gives the ability to use the video in a video editor with ease
							*/
							options.Set("x264-params", "keyint=1");
						}
					}

					auto fps = Variables::FrameRate.GetInt();
					stream->Video.OpenEncoder(fps, options.Get());

					stream->Video.WriteHeader();
				}

				movie.VideoStreams = std::move(tempstreams);
				movie.Audio = std::move(tempaudio);
			}
					
			catch (const LAV::Exception& error)
			{
				return;
			}

			catch (const LAV::ExceptionNullPtr& error)
			{
				return;
			}

			ThisHook.GetOriginal()
			(
				filename,
				flags,
				width,
				height,
				framerate,
				jpegquality,
				unk
			);

			auto enginerate = Variables::FrameRate.GetInt();

			/*
				The original function sets host_framerate to 30 so we override it
			*/
			ConVarRef hostframerate("host_framerate");
			hostframerate.SetValue(enginerate);

			movie.VideoQueue = std::make_unique<MovieData::VideoQueueType>(128);

			movie.IsStarted = true;
			BufferedFrames = 0;
			ShouldStopFrameThread = false;
			movie.FrameBufferThreadHandle = std::thread(FrameBufferThread);
		}
	}

	namespace ModuleStartMovieCommand
	{
		#pragma region Init

		void __cdecl Override
		(
			const CCommand& args
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"StartMovieCommand",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		/*
			This command is overriden to remove the incorrect description
		*/
		void __cdecl Override
		(
			const CCommand& args
		)
		{
			if (args.ArgC() < 2)
			{
				ConMsg
				(
					"SDR: Name is required for startmovie, "
					"see Github page for help\n"
				);

				return;
			}

			int width;
			int height;

			ModuleMaterialSystem::GetBackBufferDimensions
			(
				ModuleMaterialSystem::MaterialsPtr,
				nullptr,
				width,
				height
			);

			Profile::Entries.fill({});

			auto name = args[1];

			/*
				4 = FMOVIE_WAV, needed for future audio calls
			*/
			ModuleStartMovie::Override
			(
				name,
				4,
				width,
				height,
				0,
				0,
				0
			);
		}
	}

	namespace ModuleEndMovie
	{
		#pragma region Init

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"EndMovie",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		void __cdecl Override()
		{
			ThisHook.GetOriginal()();

			if (!CurrentMovie.IsStarted)
			{
				return;
			}

			ConVarRef hostframerate("host_framerate");
			hostframerate.SetValue(0);

			Msg
			(
				"SDR: Ending movie, "
				"if there are buffered frames this might take a moment\n"
			);

			auto task = concurrency::create_task([]()
			{
				if (!ShouldStopFrameThread)
				{
					ShouldStopFrameThread = true;
					CurrentMovie.FrameBufferThreadHandle.join();
				}

				if (CurrentMovie.Audio)
				{
					CurrentMovie.Audio->Finish();
				}

				/*
					Let the encoders finish all the delayed frames
				*/
				for (auto& stream : CurrentMovie.VideoStreams)
				{
					stream->Video.Finish();
				}
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

				int index = 0;

				for (const auto& entry : Profile::Entries)
				{
					if (entry.Calls > 0)
					{
						auto name = Profile::Names[index];
						auto avg = entry.TotalTime / entry.Calls;
						auto ms = avg / 1.0ms;

						Msg
						(
							"SDR: %s (%u): avg %0.4f ms\n",
							name,
							entry.Calls,
							ms
						);
					}

					++index;
				}
			});
		}
	}

	namespace ModuleSNDRecordBuffer
	{
		#pragma region Init

		void __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"SNDRecordBuffer",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		void __cdecl Override()
		{
			if (CurrentMovie.Audio)
			{
				ThisHook.GetOriginal()();
			}
		}
	}

	namespace ModuleWaveCreateTmpFile
	{
		#pragma region Init

		void __cdecl Override
		(
			const char* filename,
			int rate,
			int bits,
			int channels
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WaveCreateTmpFile",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		/*
			"rate" will always be 44100
			"bits" will always be 16
			"channels" will always be 2

			According to engine\audio\private\snd_mix.cpp @ 4003
		*/
		void __cdecl Override
		(
			const char* filename,
			int rate,
			int bits,
			int channels
		)
		{
			
		}
	}

	namespace ModuleWaveAppendTmpFile
	{
		#pragma region Init

		void __cdecl Override
		(
			const char* filename,
			void* buffer,
			int samplebits,
			int samplecount
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WaveAppendTmpFile",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		/*
			"samplebits" will always be 16

			According to engine\audio\private\snd_mix.cpp @ 4074
		*/
		void __cdecl Override
		(
			const char* filename,
			void* buffer,
			int samplebits,
			int samplecount
		)
		{
			auto bufstart = static_cast<int16_t*>(buffer);
			auto length = samplecount * samplebits / 8;

			CurrentMovie.Audio->AddPCM16Samples({bufstart, bufstart + length});
		}
	}

	namespace ModuleWaveFixupTmpFile
	{
		#pragma region Init

		void __cdecl Override
		(
			const char* filename
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WaveFixupTmpFile",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::CreateHookShort
					(
						ThisHook,
						Override,
						value
					);
				}
			)
		);

		#pragma endregion

		/*
			Gets called when the movie is ended
		*/
		void __cdecl Override
		(
			const char* filename
		)
		{
			
		}
	}
}
