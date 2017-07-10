#include "Utility.hlsl"
#include "SharedAll.hlsl"

StructuredBuffer<WorkBufferData> WorkBuffer : register(t0);
RWBuffer<uint> OutTexture : register(u0);

/*
	Parts from d3dx_dxgiformatconvert.inl
*/

inline float D3DX_Saturate_FLOAT(float _V)
{
	return min(max(_V, 0), 1);
}

inline uint D3DX_FLOAT_to_UINT(float _V, float _Scale)
{
	return (uint)floor(_V * _Scale + 0.5f);
}

inline uint D3DX_FLOAT4_to_B8G8R8A8_UNORM(precise float4 unpacked)
{
	uint packed;
	
	packed =
	(
		(D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpacked.z), 255)) |
		(D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpacked.y), 255) << 8) |
		(D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpacked.x), 255) << 16) |
		(D3DX_FLOAT_to_UINT(D3DX_Saturate_FLOAT(unpacked.w), 255) << 24)
	);

	return packed;
}

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;

	/*
		FFMpeg frames are flipped
	*/
	pos.y = Dimensions.y - pos.y - 1;

	uint index = CalculateIndex(Dimensions.x, pos);
	float3 pix = WorkBuffer[index].Color;

	OutTexture[index] = D3DX_FLOAT4_to_B8G8R8A8_UNORM(float4(pix, 1));
}
