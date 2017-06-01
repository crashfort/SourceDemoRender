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

#include <d3d9.h>
#include <d3dx9.h>

#include "Shaders\Compiled\MotionBlur_PS.hpp"
#include "Shaders\Compiled\MotionBlur_VS.hpp"

#include "materialsystem\itexture.h"
#include "shaderapi\ishaderapi.h"
#include "utlsymbol.h"

#include "view_shared.h"
#include "ivrenderview.h"
#include "iviewrender.h"

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
			OpenWaveFile,
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

				ThrowIfNull
				(
					Context,
					ExceptionType::AllocCodecContext
				);
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

				ThrowIfNull(Frame, ExceptionType::AllocAVFrame);

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

			fwrite(buffer, length, 1, WaveFile.Get());

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

		struct DirectX9Data
		{
			struct RenderTarget
			{
				void Create
				(
					IDirect3DDevice9* device,
					int width,
					int height
				)
				{
					MS::ThrowIfFailed
					(
						device->CreateTexture
						(
							width,
							height,
							1,
							D3DUSAGE_RENDERTARGET,
							D3DFMT_A8R8G8B8,
							D3DPOOL_DEFAULT,
							Texture.GetAddressOf(),
							nullptr
						)
					);

					MS::ThrowIfFailed
					(
						Texture->GetSurfaceLevel
						(
							0,
							Surface.GetAddressOf()
						)
					);
				}

				Microsoft::WRL::ComPtr<IDirect3DTexture9> Texture;
				Microsoft::WRL::ComPtr<IDirect3DSurface9> Surface;
			};

			~DirectX9Data()
			{
				if (ValveRT)
				{
					ValveRT->Release();
				}
			}

			ITexture* ValveRT = nullptr;

			RenderTarget Previous;
			RenderTarget Current;

			Microsoft::WRL::ComPtr<IDirect3DPixelShader9> PixelShader;
			Microsoft::WRL::ComPtr<IDirect3DVertexShader9> VertexShader;
		} DirectX;

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
		auto& movie = CurrentMovie;

		if (!movie.IsStarted)
		{
			return;
		}

		if (!ShouldStopFrameThread)
		{
			ShouldStopFrameThread = true;
			movie.FrameHandlerThread.join();
		}

		movie = MovieData();
	}

	namespace Module_VideoMode
	{
		namespace Types
		{
			using ReadScreenPixels = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				int x,
				int y,
				int w,
				int h,
				void* buffer,
				int format
			);
		}

		Types::ReadScreenPixels ReadScreenPixels;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"VideoMode_ReadScreenPixels",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						ReadScreenPixels,
						address
					);
				}
			)
		);
	}

	#if 0
	namespace Module_ShaderAPI
	{
		namespace Types
		{
			using ClearColor4 = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				unsigned char r,
				unsigned char g,
				unsigned char b,
				unsigned char a
			);

			using ClearBuffers = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				bool clearcol,
				bool cleardepth,
				bool clearstencil,
				int rtwidth,
				int rtheight
			);
		}

		void* ShaderAPI = nullptr;
		Types::ClearColor4 ClearColor4;
		Types::ClearBuffers ClearBuffers;

		void Set()
		{
			{
				SDR::AddressFinder address
				(
					/*
						0x100D08C6 static HL2 IDA address April 25 2017
						0x1004E547 static CSS IDA address May 30 2017

						In the function "CShaderDeviceMgrDx8::SetMode" the global variable
						"g_pShaderAPIDX8" is used.
					*/
					"materialsystem.dll",
					SDR::MemoryPattern
					(
						"\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\x83\xC4\x04\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x05\x00\x00\x00\x00\x00\x00\x00\x00\xC7\x87\x00\x00\x00\x00\x00\x00\x00\x00\x5F\x5D\xC2\x04\x00"
					),
					"xx????????xxxxx????????xx????????xx????????xxxxx",

					/*
						Increment for MOV instruction
					*/
					2
				);

				ShaderAPI = address.Get();

				SDR::ModuleShared::VerifyEntry
				(
					ShaderAPI,
					"materialsystem.dll",
					"ShaderAPI"
				);
			}

			{
				/*
					0x100BDFE0 static CSS IDA address May 30 2017
				*/
				SDR::AddressFinder address
				(
					"shaderapidx9.dll",
					SDR::MemoryPattern
					(
						"\x55\x8B\xEC\x8B\x45\x14\x0F\xB6\xD0\x8B\x45\x08\xC1"
						"\xE2\x08\x0F\xB6\xC0\x0B\xD0\x8B\x45\x0C\xC1\xE2\x08"
						"\x0F\xB6\xC0\x0B\xD0\x8B\x45\x10\xC1\xE2\x08\x0F\xB6"
						"\xC0\x0B\xD0\x89\x91\x00\x00\x00\x00\x5D\xC2\x10\x00"
					),
					"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xxxx"
				);

				SDR::ModuleShared::SetFromAddress
				(
					ClearColor4,
					address.Get()
				);

				SDR::ModuleShared::VerifyEntry
				(
					ClearColor4,
					"shaderapidx9.dll",
					"ClearColor4"
				);
			}

			{
				/*
					0x100BDCB0 static CSS IDA address May 30 2017
				*/
				SDR::AddressFinder address
				(
					"shaderapidx9.dll",
					SDR::MemoryPattern
					(
						"\x55\x8B\xEC\x83\xEC\x14\x56\x8B\xF1\x8B\x0D\x00\x00"
						"\x00\x00\x8B\x01\xFF\x50\x14\x80\x78\x4B\x00\x0F\x85"
						"\x00\x00\x00\x00\x8B\x86\x00\x00\x00\x00\x53\x8D\x9E"
						"\x00\x00\x00\x00\x8B\xCB\x8B\x80\x00\x00\x00\x00\xFF"
						"\xD0\x84\xC0\x0F\x85\x00\x00\x00\x00\x8B\x06\x8B\xCE"
					),
					"xxxxxxxxxxx????xxxxxxxxxxx????xx????xxx????xxxx????xxxxx"
					"x????xxxx"
				);

				SDR::ModuleShared::SetFromAddress
				(
					ClearBuffers,
					address.Get()
				);

				SDR::ModuleShared::VerifyEntry
				(
					ClearBuffers,
					"shaderapidx9.dll",
					"ClearBuffers"
				);
			}
		}
	}
	#endif

	namespace Module_ShaderDevice
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

			using GetBackBufferFormat = ImageFormat(__fastcall*)
			(
				void* thisptr,
				void* edx
			);
		}

		void* ShaderDevicePtr;
		Types::GetBackBufferDimensions GetBackBufferDimensions;
		Types::GetBackBufferFormat GetBackBufferFormat;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"ShaderDevice_ShaderDevicePtr",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					ShaderDevicePtr = **reinterpret_cast<void***>(address);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"ShaderDevice_GetBackBufferDimensions",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetBackBufferDimensions,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"ShaderDevice_GetBackBufferFormat",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetBackBufferFormat,
						address
					);
				}
			)
		);
	}

	namespace Module_MaterialSystem
	{
		namespace Types
		{
			using GetRenderContext = void*(__fastcall*)
			(
				void* thisptr,
				void* edx
			);

			using BeginRenderTargetAllocation = void(__fastcall*)
			(
				void* thisptr,
				void* edx
			);

			using EndRenderTargetAllocation = void(__fastcall*)
			(
				void* thisptr,
				void* edx
			);

			using CreateRenderTargetTexture = ITexture*(__fastcall*)
			(
				void* thisptr,
				void* edx,
				int width,
				int height,
				RenderTargetSizeMode_t sizemode,
				ImageFormat	format,
				MaterialRenderTargetDepth_t depth
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
		Types::GetRenderContext GetRenderContext;
		Types::BeginRenderTargetAllocation BeginRenderTargetAllocation;
		Types::EndRenderTargetAllocation EndRenderTargetAllocation;
		Types::CreateRenderTargetTexture CreateRenderTargetTexture;
		Types::BeginFrame BeginFrame;
		Types::EndFrame EndFrame;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_MaterialsPtr",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					MaterialsPtr = **reinterpret_cast<void***>(address);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_GetRenderContext",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetRenderContext,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_BeginRenderTargetAllocation",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						BeginRenderTargetAllocation,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_EndRenderTargetAllocation",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						EndRenderTargetAllocation,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_CreateRenderTargetTexture",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						CreateRenderTargetTexture,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_BeginFrame",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

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
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						EndFrame,
						address
					);
				}
			)
		);
	}

	namespace Module_RenderContext
	{
		namespace Types
		{
			using Release = int(__fastcall*)
			(
				void* thisptr,
				void* edx
			);

			using PushRenderTargetAndViewport1 = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				ITexture* texture,
				ITexture* depthtexture,
				int x,
				int y,
				int width,
				int height
			);

			using PopRenderTargetAndViewport = void(__fastcall*)
			(
				void* thisptr,
				void* edx
			);

			using ClearColor4 = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				unsigned char r,
				unsigned char g,
				unsigned char b,
				unsigned char a
			);

			using ClearBuffers = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				bool clearcol,
				bool cleardepth,
				bool clearstencil
			);
		}

		Types::Release Release;
		Types::PushRenderTargetAndViewport1 PushRenderTargetAndViewport1;
		Types::PopRenderTargetAndViewport PopRenderTargetAndViewport;
		Types::ClearColor4 ClearColor4;
		Types::ClearBuffers ClearBuffers;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"RenderContext_Release",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						Release,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"RenderContext_PushRenderTargetAndViewport1",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						PushRenderTargetAndViewport1,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"RenderContext_PopRenderTargetAndViewport",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						PopRenderTargetAndViewport,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"RenderContext_ClearBuffers",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						ClearBuffers,
						address
					);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"RenderContext_ClearColor4",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						ClearColor4,
						address
					);
				}
			)
		);
	}

	namespace Module_Texture
	{
		namespace Types
		{
			/*
				Structure from Source 2007
			*/
			struct Texture_t
			{
				enum Flags_t
				{
					IS_ALLOCATED = 0x0001,
					IS_DEPTH_STENCIL = 0x0002,
					IS_DEPTH_STENCIL_TEXTURE = 0x0004,	// depth stencil texture, not surface
					IS_RENDERABLE = (IS_DEPTH_STENCIL | IS_ALLOCATED),
					IS_FINALIZED = 0x0010,	// 360: completed async hi-res load
					IS_FAILED = 0x0020,	// 360: failed during load
					CAN_CONVERT_FORMAT = 0x0040,	// 360: allow format conversion
					IS_LINEAR = 0x0080,	// 360: unswizzled linear format
					IS_RENDER_TARGET = 0x0100,	// 360: marks a render target texture source
					IS_RENDER_TARGET_SURFACE = 0x0200,	// 360: marks a render target surface target
					IS_VERTEX_TEXTURE = 0x0800,
				};

				D3DTEXTUREADDRESS m_UTexWrap;
				D3DTEXTUREADDRESS m_VTexWrap;
				D3DTEXTUREADDRESS m_WTexWrap;
				D3DTEXTUREFILTERTYPE m_MagFilter;
				D3DTEXTUREFILTERTYPE m_MinFilter;
				D3DTEXTUREFILTERTYPE m_MipFilter;

				unsigned char m_NumLevels;
				unsigned char m_SwitchNeeded; // Do we need to advance the current copy?
				unsigned char m_NumCopies; // copies are used to optimize procedural textures
				unsigned char m_CurrentCopy; // the current copy we're using...

				int m_CreationFlags;

				CUtlSymbol m_DebugName;
				CUtlSymbol m_TextureGroupName;
				int* m_pTextureGroupCounterGlobal; // Global counter for this texture's group.
				int* m_pTextureGroupCounterFrame; // Per-frame global counter for this texture's group.

				int m_SizeBytes;
				int m_SizeTexels;
				int m_LastBoundFrame;
				int m_nTimesBoundMax;
				int m_nTimesBoundThisFrame;

				short m_Width;
				short m_Height;
				short m_Depth;
				unsigned short m_Flags;

				union
				{
					IDirect3DBaseTexture9* m_pTexture; // used when there's one copy
					IDirect3DBaseTexture9** m_ppTexture; // used when there are more than one copies
					IDirect3DSurface9* m_pDepthStencilSurface; // used when there's one depth stencil surface
					IDirect3DSurface9* m_pRenderTargetSurface[2];
				};

				ImageFormat m_ImageFormat;

				short m_Count;
				short m_CountIndex;
			};
		
			using GetTextureHandle = Texture_t*(__fastcall*)
			(
				ITexture* thisptr,
				void* edx,
				int frame,
				int texturechannel
			);
		}

		Types::GetTextureHandle GetTextureHandle;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"Texture_GetTextureHandle",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					return SDR::ModuleShared::SetFromAddress
					(
						GetTextureHandle,
						address
					);
				}
			)
		);
	}

	namespace Module_View
	{
		namespace Types
		{
			using RenderView = void(__fastcall*)
			(
				void* thisptr,
				void* edx,
				const void* view,
				int clearflags,
				int drawflags
			);
		}

		int ViewSetupOffset;
		void* ViewPtr;
		Types::RenderView RenderView;

		const void* GetViewSetup(void* thisptr)
		{
			/*
				Static CSS structure offset May 22 2017. See 101A76B0
			*/
			SDR::StructureWalker walker(thisptr);
			auto ret = walker.Advance(ViewSetupOffset);

			return ret;
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"View_ViewPtr",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					ViewPtr = **reinterpret_cast<void***>(address);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"View_RenderView",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

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
				[](rapidjson::Value& value)
				{
					try
					{
						ViewSetupOffset = SDR::ModuleShared::GetJsonOffsetInt(value);
					}

					catch (bool value)
					{
						return false;
					}

					return true;
				}
			)
		);
	}

	namespace Module_SourceGlobals
	{
		IDirect3DDevice9* Device;
		bool* DrawLoading;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"D3D9_Device",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					Device = **reinterpret_cast<IDirect3DDevice9***>(address);

					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"DrawLoading",
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					DrawLoading = *reinterpret_cast<bool**>(address);

					return true;
				}
			)
		);

		void SaveTempTexture
		(
			IDirect3DBaseTexture9* texture
		)
		{
			static int counter = 0;

			wchar_t namebuf[1024];

			swprintf_s
			(
				namebuf,
				LR"(C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Source\cstrike\Source Demo Render Output\image%d.png)",
				counter
			);

			auto hr = D3DXSaveTextureToFileW
			(
				namebuf,
				D3DXIFF_PNG,
				texture,
				nullptr
			);

			++counter;
		}


		void SaveTempTexture
		(
			ITexture* texture,
			int frame = 0,
			int channel = 0
		)
		{
			auto handle = Module_Texture::GetTextureHandle
			(
				texture,
				nullptr,
				frame,
				channel
			);

			SaveTempTexture(handle->m_pTexture);
		}
	}

	/*
		In case endmovie never gets called,
		this handles the plugin_unload
	*/
	SDR::PluginShutdownFunctionAdder A1(SDR_MovieShutdown);

	SDR::PluginStartupFunctionAdder A2("MovieRecord Setup", []()
	{
		int width;
		int height;

		Module_ShaderDevice::GetBackBufferDimensions
		(
			Module_ShaderDevice::ShaderDevicePtr,
			nullptr,
			width,
			height
		);

		auto device = Module_SourceGlobals::Device;
		auto& dx9 = CurrentMovie.DirectX;

		Module_MaterialSystem::BeginRenderTargetAllocation
		(
			Module_MaterialSystem::MaterialsPtr,
			nullptr
		);

		dx9.ValveRT = Module_MaterialSystem::CreateRenderTargetTexture
		(
			Module_MaterialSystem::MaterialsPtr,
			nullptr,
			0,
			0,
			RT_SIZE_FULL_FRAME_BUFFER,
			Module_ShaderDevice::GetBackBufferFormat
			(
				Module_ShaderDevice::ShaderDevicePtr,
				nullptr
			),
			MATERIAL_RT_DEPTH_SHARED
		);

		if (dx9.ValveRT)
		{
			dx9.ValveRT->AddRef();
		}

		Module_MaterialSystem::EndRenderTargetAllocation
		(
			Module_MaterialSystem::MaterialsPtr,
			nullptr
		);

		if (!dx9.ValveRT)
		{
			Warning
			(
				"SDR: Could not Valve rendertarget\n"
			);

			return false;
		}

		try
		{
			dx9.Previous.Create
			(
				device,
				width,
				height
			);

			dx9.Current.Create
			(
				device,
				width,
				height
			);

			MS::ThrowIfFailed
			(
				device->CreatePixelShader
				(
					(DWORD*)MotionBlur_PS_Blob,
					dx9.PixelShader.GetAddressOf()
				)
			);

			MS::ThrowIfFailed
			(
				device->CreateVertexShader
				(
					(DWORD*)MotionBlur_VS_Blob,
					dx9.VertexShader.GetAddressOf()
				)
			);
		}

		catch (HRESULT code)
		{
			auto dxerror = MAKE_D3DHRESULT(code);

			int a = 5;
			a = a;

			return false;
		}

		return true;
	});
}

namespace
{
	namespace Variables
	{
		ConVar UseSample
		(
			"sdr_render_usesample", "1", FCVAR_NEVER_AS_STRING,
			"Use frame blending",
			true, 0, true, 1
		);

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
			true, 30, false, 1000
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

		ConVar ShaderSamples
		(
			"sdr_shader_samples", "64", FCVAR_NEVER_AS_STRING,
			"Motion blur samples",
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

				if (movie.Sampler)
				{
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

				else
				{
					movie.Print(videosample.Data.data());
				}
			}
		}
	}

	namespace Module_View_Render
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
				[](rapidjson::Value& value)
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

			auto& interfaces = SDR::GetEngineInterfaces();
			auto client = interfaces.EngineClient;

			if (!CurrentMovie.IsStarted)
			{
				return;
			}

			if (*Module_SourceGlobals::DrawLoading)
			{
				return;
			}

			if (client->Con_IsVisible())
			{
				return;
			}

			Module_MaterialSystem::EndFrame
			(
				Module_MaterialSystem::MaterialsPtr,
				nullptr
			);

			Module_MaterialSystem::BeginFrame
			(
				Module_MaterialSystem::MaterialsPtr,
				nullptr,
				0
			);

			auto& movie = CurrentMovie;

			auto rendercontext = Module_MaterialSystem::GetRenderContext
			(
				Module_MaterialSystem::MaterialsPtr,
				nullptr
			);

			Module_RenderContext::PushRenderTargetAndViewport1
			(
				rendercontext,
				nullptr,
				movie.DirectX.ValveRT,
				nullptr,
				0,
				0,
				movie.Width,
				movie.Height
			);

			Module_RenderContext::ClearColor4
			(
				rendercontext,
				nullptr,
				0,
				0,
				0,
				0
			);

			Module_RenderContext::ClearBuffers
			(
				rendercontext,
				nullptr,
				true,
				false,
				false
			);

			/*Module_ShaderAPI::ClearColor4
			(
				Module_ShaderAPI::ShaderAPI,
				nullptr,
				0,
				0,
				0,
				0
			);

			Module_ShaderAPI::ClearBuffers
			(
				Module_ShaderAPI::ShaderAPI,
				nullptr,
				true,
				false,
				false,
				movie.Width,
				movie.Height
			);*/

			Module_View::RenderView
			(
				Module_View::ViewPtr,
				nullptr,
				Module_View::GetViewSetup(Module_View::ViewPtr),
				VIEW_CLEAR_STENCIL | VIEW_CLEAR_DEPTH,
				RENDERVIEW_UNSPECIFIED
			);

			Module_RenderContext::PopRenderTargetAndViewport
			(
				nullptr,
				nullptr
			);

			Module_SourceGlobals::SaveTempTexture(movie.DirectX.ValveRT);

			Module_RenderContext::Release
			(
				rendercontext,
				nullptr
			);
		}
	}

	#if 0
	namespace Module_DrawSetup
	{
		/*
			CBaseWorldView::DrawSetup in Source 2013
			\game\client\viewrender.cpp @ 5276

			0x101BA0F0 static CSS IDA address April 26 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x83\xEC\x08\xD9\x45\x08\x56\x8B\x35"
			"\x00\x00\x00\x00\x57\x8B\xF9\x89\x75\xF8\x51\xD9"
			"\x1C\x24"
		);

		auto Mask =
		(
			"xxxxxxxxxxxx????xxxxxxxxxx"
		);

		void __fastcall Override
		(
			void*,
			void*,
			float waterheight,
			int setupflags,
			float waterzadjust,
			int forceviewleaf
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"client.dll", "DrawSetup", Override, Pattern, Mask
		};

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			float waterheight,
			int setupflags,
			float waterzadjust,
			int forceviewleaf
		)
		{
			auto& globalviewid = *Module_SourceGlobals::CurrentViewID;
			
			auto savedview = globalviewid;

			ThisHook.GetOriginal()
			(
				thisptr,
				edx,
				waterheight,
				setupflags,
				waterzadjust,
				forceviewleaf
			);

			/*
				VIEW_ILLEGAL = -2, \game\client\viewrender.h
			*/
			globalviewid = -2;

			/*
				VIEW_MAIN = 0, \game\client\viewrender.h
			*/
			if (globalviewid == 0)
			{
				globalviewid = 1000;

				auto rendercontext = materials->GetRenderContext();

				rendercontext->ClearColor4ub(255, 255, 255, 255);
			}

			globalviewid = savedview;
		}
	}
	#endif

	#if 0
	namespace Module_PerformScreenSpaceEffects
	{
		#pragma region Init

		/*
			0x101BD810 static CSS IDA address April 22 2017
		*/
		auto Pattern = SDR::MemoryPattern
		(
			"\x55\x8B\xEC\x83\xEC\x08\x8B\x0D\x00\x00\x00\x00"
			"\x53\x56\x57\x33\xF6\x33\xFF\x89\x75\xF8\x89\x7D"
			"\xFC\x8B\x01\x85\xC0\x74\x36\x68\x00\x00\x00\x00"
			"\x68\x00\x00\x00\x00\x68\x00\x00\x00\x00\x68\x00"
			"\x00\x00\x00\x68\x00\x00\x00\x00\x57\x57\x57\x57"
			"\x8D\x4D\xF8\x51\x50\x8B\x40\x50\xFF\xD0\x8B\x7D"
			"\xFC\x83\xC4\x2C\x8B\x75\xF8\x8B\x0D\x00\x00\x00"
			"\x00\x8B\x19\x8B\x0D\x00\x00\x00\x00\x8B\x01\x8B"
			"\x80\x00\x00\x00\x00\xFF\xD0"
		);

		auto Mask =
		(
			"xxxxxxxx????xxxxxxxxxxxxxxxxxxxx????x????x????x?"
			"???x????xxxxxxxxxxxxxxxxxxxxxxxxx????xxxx????xxx"
			"x????xx"
		);

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			int x,
			int y,
			int w,
			int h
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleMask<ThisFunction> ThisHook
		{
			"client.dll", "PerformScreenSpaceEffects", Override, Pattern, Mask
		};

		#pragma endregion

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			int x,
			int y,
			int w,
			int h
		)
		{
			ThisHook.GetOriginal()
			(
				thisptr,
				edx,
				x,
				y,
				w,
				h
			);

			auto& interfaces = SDR::GetEngineInterfaces();
			auto client = interfaces.EngineClient;

			if (!CurrentMovie.IsStarted)
			{
				return;
			}

			if (*Module_SourceGlobals::DrawLoading)
			{
				return;
			}

			if (client->Con_IsVisible())
			{
				return;
			}

			auto& movie = CurrentMovie;
			auto& dx9 = movie.DirectX;
			auto device = Module_SourceGlobals::Device;

			auto rendercontext = materials->GetRenderContext();

			#if 0
			static int counter = 0;

			wchar_t namebuf[1024];

			swprintf_s
			(
				namebuf,
				LR"(C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Source\cstrike\Source Demo Render Output\image%d.png)",
				counter
			);

			auto hr = D3DXSaveTextureToFileW
			(
				namebuf,
				D3DXIFF_PNG,
				textureptr->m_pTexture,
				nullptr
			);

			++counter;
			#endif

			int a = 5;
			a = a;
		}
	}
	#endif

	#if 0
	namespace Module_GetFullFrameDepthTexture
	{
		/*
			GetFullFrameDepthTexture, \game\client\rendertexture.cpp @ 55

			0x101ADD0E static CSS IDA address April 25 2017
		*/
		SDR::RelativeJumpFunctionFinder Address
		{
			SDR::AddressFinder
			(
				"client.dll",
				SDR::MemoryPattern
				(
					"\xE8\x00\x00\x00\x00\x8B\x0D\x00\x00\x00\x00\x8B"
					"\xD8\x89\x5D\xFC\x8B\x11\xFF\x92\x00\x00\x00\x00"
					"\x8B\xF0\x85\xF6\x74\x07\x8B\x06\x8B\xCE\xFF\x50"
					"\x08"
				),
				"x????xx????xxxxxxxxx????xxxxxxxxxxxxx"
			)
		};

		ITexture* __cdecl Override();

		using ThisFunction = decltype(Override)*;

		SDR::HookModuleStaticAddress<ThisFunction> ThisHook
		{
			"client.dll", "GetFullFrameDepthTexture", Override, Address.Get()
		};

		ITexture* __cdecl Override()
		{
			return CurrentMovie.DirectX9.DepthTexture;
		}
	}
	#endif

	namespace Module_StartMovie
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
				[](rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					SDR::RelativeJumpFunctionFinder jumper(address);

					try
					{
						SDR::CreateHook
						(
							ThisHook,
							Override,
							jumper.Get()
						);
					}

					catch (MH_STATUS status)
					{
						return false;
					}

					return true;
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
				
			movie.Width = width;
			movie.Height = height;

			{
				try
				{
					av_log_set_callback(LAV::LogFunction);

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

					/*
						User selected pixel format does not match any in config
					*/
					if (pxformat == AV_PIX_FMT_NONE)
					{
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

			auto enginerate = Variables::SamplesPerSecond.GetInt();

			if (!Variables::UseSample.GetBool())
			{
				enginerate = Variables::FrameRate.GetInt();
			}

			/*
				The original function sets host_framerate to 30 so we override it
			*/
			ConVarRef hostframerate("host_framerate");
			hostframerate.SetValue(enginerate);

			if (Variables::UseSample.GetBool())
			{
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

				movie.Sampler = std::make_unique<SDR::Sampler::EasyByteSampler>
				(
					settings,
					framepitch,
					&movie
				);
			}

			movie.IsStarted = true;

			movie.FramesToSampleBuffer = std::make_unique<MovieData::VideoQueueType>(128);

			ShouldStopFrameThread = false;
			movie.FrameHandlerThread = std::thread(FrameThreadHandler);
		}
	}

	namespace Module_StartMovieCommand
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
				[](rapidjson::Value& value)
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

			Module_ShaderDevice::GetBackBufferDimensions
			(
				Module_ShaderDevice::ShaderDevicePtr,
				nullptr,
				width,
				height
			);

			auto name = args[1];

			/*
				4 = FMOVIE_WAV, needed for future audio calls
			*/
			Module_StartMovie::Override
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

	namespace Module_EndMovie
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
				[](rapidjson::Value& value)
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
		#pragma region Init

		void __fastcall Override
		(
			void* thisptr,
			void* edx,
			void* info
		);

		using ThisFunction = decltype(Override)*;

		SDR::HookModule<ThisFunction> ThisHook;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"WriteMovieFrame",
				[](rapidjson::Value& value)
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
			void* thisptr,
			void* edx,
			void* info
		)
		{
			return;

			auto width = CurrentMovie.Width;
			auto height = CurrentMovie.Height;

			auto& movie = CurrentMovie;

			auto& time = movie.SamplingTime;

			MovieData::VideoFutureSampleData newsample;
			newsample.Time = time;
			newsample.Data.resize(movie.GetRGB24ImageSize());

			auto pxformat = IMAGE_FORMAT_RGB888;

			if (movie.Video->CodecContext->pix_fmt == AV_PIX_FMT_BGR24)
			{
				pxformat = IMAGE_FORMAT_BGR888;
			}

			/*
				This has been reverted to again,
				in newer games like TF2 the materials are handled much differently
				but this endpoint function remains the same. Less elegant but what you gonna do.
			*/
			Module_VideoMode::ReadScreenPixels
			(
				thisptr,
				edx,
				0,
				0,
				width,
				height,
				newsample.Data.data(),
				pxformat
			);

			auto buffersize = Variables::FrameBufferSize.GetInt();

			/*
				Encoder is falling behind, too much input with too little output
			*/
			if (movie.BufferedFrames > buffersize)
			{
				Warning
				(
					"SDR: Too many buffered frames, waiting for encoder\n"
				);

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

			if (movie.Sampler)
			{
				auto spsvar = static_cast<double>(Variables::SamplesPerSecond.GetInt());
				auto sampleframerate = 1.0 / spsvar;

				time += sampleframerate;
			}
		}
	}

	namespace Module_SNDRecordBuffer
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
				[](rapidjson::Value& value)
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

	namespace Module_WaveCreateTmpFile
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
				[](rapidjson::Value& value)
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

	namespace Module_WaveAppendTmpFile
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
				[](rapidjson::Value& value)
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

	namespace Module_WaveFixupTmpFile
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
				[](rapidjson::Value& value)
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
