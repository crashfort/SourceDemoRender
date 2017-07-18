#include "Utility.hlsl"
#include "SharedAll.hlsl"

RWStructuredBuffer<WorkBufferData> InputBuffer : register(u0);

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(Dimensions.x, pos);

	InputBuffer[index].Color = float3(0, 0, 0);
}
