#pragma once
#include <d3d9.h>
#include <d3d11.h>
#include <wrl.h>
#include <memory>

#include "FutureData.hpp"
#include "ConversionBase.hpp"
#include "Video.hpp"
#include "readerwriterqueue.h"

namespace SDR::Stream
{
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
			std::unique_ptr<D3D11::ConversionBase> ConversionPtr;

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
