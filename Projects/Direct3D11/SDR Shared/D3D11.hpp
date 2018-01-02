#pragma once
#include <d3d11.h>
#include <wrl.h>

namespace SDR::D3D11
{
	struct BlobData
	{
		template <size_t Size>
		static BlobData Make(const BYTE(&data)[Size])
		{
			return Make(data, Size);
		}

		static BlobData Make(const BYTE* data, size_t size)
		{
			BlobData ret;
			ret.Data = data;
			ret.Size = size;

			return ret;
		}

		const BYTE* Data;
		size_t Size;
	};

	void OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11ComputeShader** shader);
	void OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11VertexShader** shader);
	void OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11PixelShader** shader);

	namespace Shader
	{
		template <size_t Count>
		inline void CSResetSRV(ID3D11DeviceContext* context, unsigned int index)
		{
			ID3D11ShaderResourceView* nullsrvs[Count] = {};
			context->CSSetShaderResources(index, Count, nullsrvs);
		}

		template <size_t Count>
		inline void CSResetUAV(ID3D11DeviceContext* context, unsigned int index)
		{
			ID3D11UnorderedAccessView* nulluavs[Count] = {};
			context->CSSetUnorderedAccessViews(index, Count, nulluavs, nullptr);
		}

		template <size_t Count>
		inline void CSResetCBV(ID3D11DeviceContext* context, unsigned int index)
		{
			ID3D11Buffer* nullcbufs[Count] = {};
			context->CSSetConstantBuffers(index, Count, nullcbufs);
		}

		template <size_t Count>
		inline void PSResetSRV(ID3D11DeviceContext* context, unsigned int index)
		{
			ID3D11ShaderResourceView* nullsrvs[Count] = {};
			context->PSSetShaderResources(index, Count, nullsrvs);
		}

		template <size_t Count>
		inline void PSResetUAV(ID3D11DeviceContext* context, unsigned int index)
		{
			ID3D11UnorderedAccessView* nulluavs[Count] = {};
			context->PSSetUnorderedAccessViews(index, Count, nulluavs, nullptr);
		}

		template <size_t Count>
		inline void PSResetCBV(ID3D11DeviceContext* context, unsigned int index)
		{
			ID3D11Buffer* nullcbufs[Count] = {};
			context->PSSetConstantBuffers(index, Count, nullcbufs);
		}
	}
}
