#include "PrecompiledHeader.hpp"
#include "Interface\Application\Application.hpp"

#include "dbg.h"

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

		void OpenEncoder
		(
			int width,
			int height,
			int framerate,
			AVPixelFormat pxformat,
			AVColorSpace colorspace,
			AVColorRange colorrange,
			AVDictionary** options
		)
		{
			CodecContext->width = width;
			CodecContext->height = height;
			CodecContext->pix_fmt = pxformat;
			CodecContext->colorspace = colorspace;
			CodecContext->color_range = colorrange;

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

		void SetYUVInput
		(
			uint8_t* bufy,
			uint8_t* bufu,
			uint8_t* bufv
		)
		{
			Frame->data[0] = bufy;
			Frame->data[1] = bufu;
			Frame->data[2] = bufv;
		}

		void SetRGBInput(uint8_t* buf)
		{
			Frame->data[0] = buf;
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
		}
		
		void* MaterialsPtr;
		Types::GetBackBufferDimensions GetBackBufferDimensions;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_MaterialsPtr",
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
				{
					auto address = SDR::GetAddressFromJsonFlex(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetBackBufferDimensions,
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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

		std::unique_ptr<SDRVideoWriter> Video;
		std::unique_ptr<SDRAudioWriter> Audio;

		struct YUVFutureData
		{
			std::vector<uint8_t> Y;
			std::vector<uint8_t> U;
			std::vector<uint8_t> V;
		};

		struct RGBFutureData
		{
			std::vector<uint8_t> RGB;
		};

		using YUVQueueType = moodycamel::ReaderWriterQueue<YUVFutureData>;
		using RGBQueueType = moodycamel::ReaderWriterQueue<RGBFutureData>;

		std::thread FrameBufferThreadHandle;
		std::unique_ptr<YUVQueueType> YUVQueue;
		std::unique_ptr<RGBQueueType> RGBQueue;

		struct DirectX9Data
		{
			struct RenderTarget
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

			RenderTarget SharedRT;
		} DirectX9;

		struct DirectX11Data
		{
			struct ConversionRuleData
			{
				ConversionRuleData() = default;

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

			struct YUVBuffer
			{
				struct Entry
				{
					void Create(ID3D11Device* device, int size)
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
						viewdesc.Format = DXGI_FORMAT_R8_UINT;
						viewdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
						viewdesc.Buffer.NumElements = size;

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
						auto hr = context->Map
						(
							Buffer.Get(),
							0,
							D3D11_MAP_READ,
							0,
							mapped
						);

						return hr;
					}

					void UnMap(ID3D11DeviceContext* context)
					{
						context->Unmap
						(
							Buffer.Get(),
							0
						);
					}

					Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
					Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> View;
				};

				void Create
				(
					ID3D11Device* device,
					int sizey,
					int sizeu,
					int sizev
				)
				{
					Y.Create(device, sizey);
					U.Create(device, sizeu);
					V.Create(device, sizev);
				}

				Entry Y;
				Entry U;
				Entry V;
			};

			void Create(HANDLE dx9handle, AVFrame* baseyuv)
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
						Device.GetAddressOf(),
						0,
						Context.GetAddressOf()
					)
				);

				Microsoft::WRL::ComPtr<ID3D11Resource> tempresource;

				MS::ThrowIfFailed
				(
					Device->OpenSharedResource
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
					Device->CreateShaderResourceView
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
						file.Assign
						(
							path,
							"rb"
						);
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

				ConversionRuleData table[] =
				{
					ConversionRuleData(AV_PIX_FMT_YUV420P, "RGB32_To_YUV420", true),
					ConversionRuleData(AV_PIX_FMT_YUV444P, "RGB32_To_YUV444", true),

					/*
						libx264rgb
					*/
					ConversionRuleData(AV_PIX_FMT_BGR0, "BGRA_PackRemoveAlpha", false),
				};

				bool found = false;

				for (auto&& entry : table)
				{
					if (entry.Format == baseyuv->format)
					{
						ConversionRule = entry;

						openshader
						(
							Device.Get(),
							entry.ShaderName,
							ComputeShader.GetAddressOf()
						);

						found = true;
						break;
					}
				}

				if (!found)
				{
					Warning
					(
						"SDR: No conversion rule found for %s\n",
						av_get_pix_fmt_name((AVPixelFormat)baseyuv->format)
					);

					throw false;
				}

				if (ConversionRule.IsYUV)
				{
					YUVBuffer.Create
					(
						Device.Get(),
						baseyuv->buf[0]->size,
						baseyuv->buf[1]->size,
						baseyuv->buf[2]->size
					);

					__declspec(align(16)) struct
					{
						int Strides[3];
					} constantbufferdata;

					constantbufferdata.Strides[0] = baseyuv->linesize[0];
					constantbufferdata.Strides[1] = baseyuv->linesize[1];
					constantbufferdata.Strides[2] = baseyuv->linesize[2];

					D3D11_BUFFER_DESC cbufdesc = {};
					cbufdesc.ByteWidth = sizeof(constantbufferdata);
					cbufdesc.Usage = D3D11_USAGE_DEFAULT;
					cbufdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

					D3D11_SUBRESOURCE_DATA cbufsubdesc = {};
					cbufsubdesc.pSysMem = &constantbufferdata;

					MS::ThrowIfFailed
					(
						Device->CreateBuffer
						(
							&cbufdesc,
							&cbufsubdesc,
							YUVConstantBuffer.GetAddressOf()
						)
					);
				}

				else
				{
					auto size = baseyuv->buf[0]->size;

					D3D11_TEXTURE2D_DESC texdesc;
					SharedTexture->GetDesc(&texdesc);

					D3D11_BUFFER_DESC desc = {};
					desc.ByteWidth = size * sizeof(uint32_t);
					desc.Usage = D3D11_USAGE_DEFAULT;
					desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
					desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
					desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
					desc.StructureByteStride = sizeof(uint32_t);

					MS::ThrowIfFailed
					(
						Device->CreateBuffer
						(
							&desc,
							nullptr,
							TempRGBBuffer.GetAddressOf()
						)
					);

					D3D11_UNORDERED_ACCESS_VIEW_DESC viewdesc = {};
					viewdesc.Format = DXGI_FORMAT_UNKNOWN;
					viewdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
					viewdesc.Buffer.NumElements = size / sizeof(uint32_t);

					MS::ThrowIfFailed
					(
						Device->CreateUnorderedAccessView
						(
							TempRGBBuffer.Get(),
							&viewdesc,
							TempRGBBufferUAV.GetAddressOf()
						)
					);
				}
			}

			ConversionRuleData ConversionRule;

			Microsoft::WRL::ComPtr<ID3D11Device> Device;
			Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;
			Microsoft::WRL::ComPtr<ID3D11ComputeShader> ComputeShader;

			Microsoft::WRL::ComPtr<ID3D11Buffer> YUVConstantBuffer;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> SharedTexture;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SharedTextureSRV;

			Microsoft::WRL::ComPtr<ID3D11Buffer> TempRGBBuffer;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> TempRGBBufferUAV;
			
			YUVBuffer YUVBuffer;
		} DirectX11;
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
		auto yuv = movie.DirectX11.ConversionRule.IsYUV;

		while (!ShouldStopFrameThread)
		{
			if (yuv)
			{
				MovieData::YUVFutureData item;

				while (movie.YUVQueue->try_dequeue(item))
				{
					--BufferedFrames;

					movie.Video->SetYUVInput(item.Y.data(), item.U.data(), item.V.data());
					movie.Video->SendRawFrame();
				}
			}

			else
			{
				MovieData::RGBFutureData item;

				while (movie.RGBQueue->try_dequeue(item))
				{
					--BufferedFrames;

					movie.Video->SetRGBInput(item.RGB.data());
					movie.Video->SendRawFrame();
				}
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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

			auto& movie = CurrentMovie;
			auto& dx9 = movie.DirectX9;
			auto& dx11 = movie.DirectX11;
			auto& interfaces = SDR::GetEngineInterfaces();
			auto client = interfaces.EngineClient;

			if (!CurrentMovie.IsStarted)
			{
				return;
			}

			if (*ModuleSourceGlobals::DrawLoading)
			{
				return;
			}

			if (client->Con_IsVisible())
			{
				return;
			}

			Profile::ScopedEntry e1(Profile::Types::ViewRender);

			HRESULT hr;
			Microsoft::WRL::ComPtr<IDirect3DSurface9> surface;

			hr = ModuleSourceGlobals::DX9Device->GetRenderTarget
			(
				0,
				surface.GetAddressOf()
			);

			if (FAILED(hr))
			{
				Warning
				(
					"SDR: Could not get DX9 RT\n"
				);

				return;
			}

			/*
				The DX11 texture now contains this data
			*/
			hr = ModuleSourceGlobals::DX9Device->StretchRect
			(
				surface.Get(),
				nullptr,
				dx9.SharedRT.Surface.Get(),
				nullptr,
				D3DTEXF_NONE
			);

			if (FAILED(hr))
			{
				Warning
				(
					"SDR: Could not copy DX9 RT -> DX11 RT\n"
				);

				return;
			}

			/*
				Convert RGBA32 to wanted format
			*/
			{
				Profile::ScopedEntry e1(Profile::Types::DX11Proc);

				HRESULT hr;
				auto& context = dx11.Context;

				if (dx11.ConversionRule.IsYUV)
				{
					std::initializer_list<ID3D11ShaderResourceView*> srvs =
					{
						dx11.SharedTextureSRV.Get()
					};

					context->CSSetShaderResources(0, srvs.size(), srvs.begin());

					std::initializer_list<ID3D11UnorderedAccessView*> uavs =
					{
						dx11.YUVBuffer.Y.View.Get(),
						dx11.YUVBuffer.U.View.Get(),
						dx11.YUVBuffer.V.View.Get(),
					};

					context->CSSetUnorderedAccessViews(0, uavs.size(), uavs.begin(), nullptr);
				}

				else
				{
					std::initializer_list<ID3D11ShaderResourceView*> srvs =
					{
						dx11.SharedTextureSRV.Get()
					};

					context->CSSetShaderResources(0, srvs.size(), srvs.begin());

					std::initializer_list<ID3D11UnorderedAccessView*> uavs =
					{
						dx11.TempRGBBufferUAV.Get(),
					};

					context->CSSetUnorderedAccessViews(0, uavs.size(), uavs.begin(), nullptr);
				}

				context->Dispatch
				(
					std::ceil(movie.Width / 8.0),
					std::ceil(movie.Height / 8.0),
					1
				);

				if (dx11.ConversionRule.IsYUV)
				{
					Profile::ScopedEntry e1(Profile::Types::PushYUV);

					D3D11_MAPPED_SUBRESOURCE mappedy;
					D3D11_MAPPED_SUBRESOURCE mappedu;
					D3D11_MAPPED_SUBRESOURCE mappedv;

					auto hrs =
					{
						dx11.YUVBuffer.Y.Map(context.Get(), &mappedy),
						dx11.YUVBuffer.U.Map(context.Get(), &mappedu),
						dx11.YUVBuffer.V.Map(context.Get(), &mappedv)
					};

					bool pass = true;

					for (auto res : hrs)
					{
						if (FAILED(res))
						{
							pass = false;

							Warning
							(
								"SDR: Could not map DX11 YUV buffers\n"
							);

							break;
						}
					}

					if (pass)
					{
						auto ptry = (uint8_t*)mappedy.pData;
						auto ptru = (uint8_t*)mappedu.pData;
						auto ptrv = (uint8_t*)mappedv.pData;

						MovieData::YUVFutureData item;						
						item.Y.assign(ptry, ptry + mappedy.RowPitch);
						item.U.assign(ptru, ptru + mappedu.RowPitch);
						item.V.assign(ptrv, ptrv + mappedv.RowPitch);

						++BufferedFrames;
						movie.YUVQueue->enqueue(std::move(item));
					}

					dx11.YUVBuffer.Y.UnMap(context.Get());
					dx11.YUVBuffer.U.UnMap(context.Get());
					dx11.YUVBuffer.V.UnMap(context.Get());
				}

				else
				{
					Profile::ScopedEntry e1(Profile::Types::PushRGB);

					D3D11_MAPPED_SUBRESOURCE mapped;

					hr = context->Map
					(
						dx11.TempRGBBuffer.Get(),
						0,
						D3D11_MAP_READ,
						0,
						&mapped
					);

					if (FAILED(hr))
					{
						Warning
						(
							"SDR: Could not map DX11 RGB buffer\n"
						);
					}

					else
					{
						auto ptr = (uint8_t*)mapped.pData;

						MovieData::RGBFutureData item;
						item.RGB.assign(ptr, ptr + mapped.RowPitch);

						++BufferedFrames;
						movie.RGBQueue->enqueue(std::move(item));
					}

					context->Unmap(dx11.TempRGBBuffer.Get(), 0);
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
					Warning
					(
						"SDR: Movie output path is invalid\n"
					);

					return;
				}

				case ERROR_CANCELLED:
				{
					Warning
					(
						"SDR: Extra directories were created but are hidden, aborting\n"
					);

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
				AVCodec* Encoder;

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

			const VideoConfigurationData* vidconfig = nullptr;
			auto colorspace = AVCOL_SPC_BT470BG;
			auto colorrange = AVCOL_RANGE_MPEG;
			auto pxformat = AV_PIX_FMT_NONE;
				
			movie.Width = width;
			movie.Height = height;

			{
				try
				{
					av_log_set_callback(LAV::LogFunction);

					auto tempvideo = std::make_unique<SDRVideoWriter>();
					std::unique_ptr<SDRAudioWriter> tempaudio;

					if (Variables::Audio::Enable.GetBool())
					{
						tempaudio = std::make_unique<SDRAudioWriter>();
					}

					auto vidwriter = tempvideo.get();
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
							auto tester = config.Encoder->name;

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
							V_StripExtension
							(
								finalfilename,
								finalfilename,
								sizeof(finalfilename)
							);

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

						vidwriter->Frame.Assign
						(
							width,
							height,
							pxformat,
							colorspace,
							colorrange
						);

						try
						{
							auto& dx9 = movie.DirectX9;
							auto& dx11 = movie.DirectX11;

							dx9.SharedRT.Create
							(
								ModuleSourceGlobals::DX9Device,
								width,
								height
							);

							dx11.Create
							(
								movie.DirectX9.SharedRT.SharedHandle,
								vidwriter->Frame.Get()
							);

							if (dx11.ConversionRule.IsYUV)
							{
								std::initializer_list<ID3D11Buffer*> cbufs =
								{
									dx11.YUVConstantBuffer.Get()
								};

								dx11.Context->CSSetConstantBuffers
								(
									0,
									cbufs.size(),
									cbufs.begin()
								);
							}

							dx11.Context->CSSetShader
							(
								dx11.ComputeShader.Get(),
								nullptr,
								0
							);
						}

						catch (HRESULT status)
						{
							return;
						}

						catch (bool value)
						{
							return;
						}

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

						vidwriter->SetEncoder(vidconfig->Encoder);
					}

					{
						LAV::ScopedAVDictionary options;
						AVDictionary** dictptr = nullptr;

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

							dictptr = options.Get();
						}

						auto fps = Variables::FrameRate.GetInt();

						vidwriter->OpenEncoder
						(
							width,
							height,
							fps,
							pxformat,
							colorspace,
							colorrange,
							dictptr
						);
					}

					vidwriter->WriteHeader();

					movie.Video = std::move(tempvideo);
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

			if (movie.DirectX11.ConversionRule.IsYUV)
			{
				movie.YUVQueue = std::make_unique<MovieData::YUVQueueType>(128);
			}

			else
			{
				movie.RGBQueue = std::make_unique<MovieData::RGBQueueType>(128);
			}

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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
				[]
				(
					const char* name,
					rapidjson::Value& value
				)
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
