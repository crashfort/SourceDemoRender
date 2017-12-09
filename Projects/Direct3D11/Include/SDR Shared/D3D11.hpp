#pragma once
#include <d3d11.h>
#include <wrl.h>

namespace SDR::D3D11
{
	struct BlobData
	{
		const BYTE* Data;
		size_t Size;
	};

	template <size_t Size>
	BlobData MakeBlob(const BYTE(&data)[Size])
	{
		BlobData ret;
		ret.Data = data;
		ret.Size = Size;

		return ret;
	}

	inline BlobData MakeBlob(const BYTE* data, size_t size)
	{
		BlobData ret;
		ret.Data = data;
		ret.Size = size;

		return ret;
	}

	void OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11ComputeShader** shader);
	void OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11VertexShader** shader);
	void OpenShader(ID3D11Device* device, const char* name, const BlobData& blob, ID3D11PixelShader** shader);
}
