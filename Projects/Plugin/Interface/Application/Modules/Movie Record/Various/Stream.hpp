#pragma once
#include <d3d9.h>
#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "Video.hpp"
#include "Pool.hpp"

#include "readerwriterqueue.h"

namespace SDR::Stream
{
	/*
		This structure is sent to the encoder thread from the capture thread.
	*/
	struct FutureData
	{
		SDR::Video::Writer* Writer;
		
		SDR::Pool::FramePool* PoolPtr;
		std::array<SDR::Pool::HandleData, 3> PoolHandles;
	};

	/*
		A lock-free producer/consumer queue.
	*/
	using QueueType = moodycamel::ReaderWriterQueue<FutureData>;

	struct SharedData
	{
		struct DirectX11Data
		{
			void Create(int width, int height, bool sampling);

			Microsoft::WRL::ComPtr<ID3D11Device> Device;
			Microsoft::WRL::ComPtr<ID3D11DeviceContext> Context;

			int GroupsX;
			int GroupsY;

			/*
				Contains the current video frame dimensions. Will always be bound at slot 0.
			*/
			Microsoft::WRL::ComPtr<ID3D11Buffer> SharedConstantBuffer;

			__declspec(align(16)) struct
			{
				float Weight;
			} SamplingConstantData;

			Microsoft::WRL::ComPtr<ID3D11Buffer> SamplingConstantBuffer;
			Microsoft::WRL::ComPtr<ID3D11ComputeShader> SamplingShader;

			/*
				Shader for setting every UAV structure color to 0.
			*/
			Microsoft::WRL::ComPtr<ID3D11ComputeShader> ClearShader;

			/*
				When no sampling is enabled, this shader just takes the game backbuffer texture and puts it into WorkBuffer.
			*/
			Microsoft::WRL::ComPtr<ID3D11ComputeShader> PassShader;
		} DirectX11;
	};

	struct StreamBase
	{
		struct DirectX9Data
		{
			struct SharedSurfaceData
			{
				void Create(IDirect3DDevice9Ex* device, int width, int height);

				HANDLE SharedHandle = nullptr;
				Microsoft::WRL::ComPtr<IDirect3DTexture9> Texture;
				Microsoft::WRL::ComPtr<IDirect3DSurface9> Surface;
			};

			void Create(IDirect3DDevice9Ex* device, int width, int height);

			/*
				This is the surface that we draw on to.
				It is shared with a DirectX 11 texture so we can run it through shaders.
			*/
			SharedSurfaceData SharedSurface;
		} DirectX9;

		struct DirectX11Data
		{
			/*
				Base for hardware conversion routines.
			*/
			struct ConversionBase
			{
				virtual ~ConversionBase() = default;

				virtual void Create(ID3D11Device* device, AVFrame* reference, bool staging) = 0;

				/*
					States that need update every frame.
				*/
				virtual void DynamicBind(ID3D11DeviceContext* context) = 0;

				/*
					Try to retrieve data to CPU after an operation.
				*/
				virtual bool Download(ID3D11DeviceContext* context, FutureData& item) = 0;
			};

			/*
				Hardware conversion shaders will store their data in this type.
				It's readable by the CPU and the finished frame is expected to be in
				the right format.
			*/
			struct GPUBuffer
			{
				void Create(ID3D11Device* device, DXGI_FORMAT viewformat, size_t size, int numelements, bool staging);

				HRESULT Map(ID3D11DeviceContext* context, D3D11_MAPPED_SUBRESOURCE* mapped);
				void Unmap(ID3D11DeviceContext* context);

				bool Staging;
				size_t Size;
				Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
				Microsoft::WRL::ComPtr<ID3D11Buffer> BufferStaging;
				Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> View;
			};

			struct ConversionBGR0 : ConversionBase
			{
				virtual void Create(ID3D11Device* device, AVFrame* reference, bool staging) override;
				virtual void DynamicBind(ID3D11DeviceContext* context) override;
				virtual bool Download(ID3D11DeviceContext* context, FutureData& item) override;

				GPUBuffer Buffer;
			};

			struct ConversionYUV : ConversionBase
			{
				virtual void Create(ID3D11Device* device, AVFrame* reference, bool staging) override;
				virtual void DynamicBind(ID3D11DeviceContext* context) override;
				virtual bool Download(ID3D11DeviceContext* context, FutureData& item) override;

				GPUBuffer Y;
				GPUBuffer U;
				GPUBuffer V;

				Microsoft::WRL::ComPtr<ID3D11Buffer> ConstantBuffer;
			};

			void Create(ID3D11Device* device, HANDLE dx9handle, AVFrame* reference, bool staging);

			/*
				Between CS dispatches the resources should be unbound.
			*/
			void ResetShaderInputs(ID3D11DeviceContext* context);

			/*
				Weighs a new engine frame onto the existing work buffer.
			*/
			void NewFrame(SharedData& shared, float weight);

			/*
				Clears the work buffer to black color.
			*/
			void Clear(SharedData& shared);

			/*
				Pass the latest engine frame directly into the work buffer.
			*/
			void Pass(SharedData& shared);

			/*
				Converts to user format.
			*/
			void Conversion(SharedData& shared);

			bool Download(SharedData& shared, FutureData& item);

			void Dispatch(const SharedData& shared);

			/*
				The newest and freshest frame provided by the engine.
			*/
			Microsoft::WRL::ComPtr<ID3D11Texture2D> SharedTexture;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SharedTextureSRV;

			/*
				Format specific buffer for format conversions. Handles
				binding shader resources and downloading the finished frame.
			*/
			std::unique_ptr<ConversionBase> ConversionPtr;

			/*
				Varying shader, handled by FrameBuffer. Using frame data from WorkBuffer,
				this shader will write into the varying bound resources.
			*/
			Microsoft::WRL::ComPtr<ID3D11ComputeShader> ConversionShader;

			/*
				Data that will be sent off for conversion. This buffer is of type
				WorkBufferData both on the CPU and GPU.
			*/
			Microsoft::WRL::ComPtr<ID3D11Buffer> WorkBuffer;
			Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> WorkBufferUAV;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> WorkBufferSRV;
		} DirectX11;

		SDR::Video::Writer Video;
		SDR::Pool::FramePool FramePool;

		/*
			Skip first frame as it will always be black when capturing the engine backbuffer.
		*/
		bool FirstFrame = true;

		struct
		{
			double Remainder = 0;
		} SamplingData;
	};
}
