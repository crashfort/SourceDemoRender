#include "Utility.hlsl"
#include "SharedAll.hlsl"

Texture2D<float4> SharedTexture : register(t0);
RWStructuredBuffer<WorkBufferData> WorkBuffer : register(u0);

cbuffer InputData : register(b1)
{
	float Weight;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;
	
	/*
		FFMpeg frames are flipped
	*/
	pos.y = Dimensions.y - pos.y - 1;

	uint index = CalculateIndex(Dimensions.x, pos);

	float3 newpix = SharedTexture.Load(dtid);
	WorkBuffer[index].Color += Weight * newpix;
}
