#include "Utility.hlsl"
#include "SharedAll.hlsl"

Texture2D<float4> SharedTexture : register(t0);
RWStructuredBuffer<WorkBufferData> WorkBuffer : register(u0);

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(Dimensions.x, pos);
	float3 newpix = SharedTexture.Load(dtid);

	WorkBuffer[index].Color = newpix;
}
