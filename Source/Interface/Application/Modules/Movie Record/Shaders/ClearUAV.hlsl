#include "Utility.hlsl"
#include "SharedAll.hlsl"

RWStructuredBuffer<float3> InputBuffer : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(Dimensions.x, pos);

	InputBuffer[index] = float3(0, 0, 0);
}
