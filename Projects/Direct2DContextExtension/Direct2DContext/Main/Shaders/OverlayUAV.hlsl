#include <Utility.hlsl>
#include <SharedAll.hlsl>

Texture2D<float4> Direct2DTextureSRV : register(t0);
RWStructuredBuffer<WorkBufferData> GameFrameUAV : register(u0);

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(Dimensions.x, pos);
	float3 newpix = Direct2DTextureSRV.Load(dtid);

	GameFrameUAV[index].Color += newpix;
}
