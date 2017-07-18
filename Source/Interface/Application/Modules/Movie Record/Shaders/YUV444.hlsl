#include "YUVShared.hlsl"
#include "Utility.hlsl"

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	int width = Dimensions.x;
	int height = Dimensions.y;
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(width, pos);
	float3 pix = WorkBuffer[index].Color;

	float y = 16 + pix.r * 65.481 + pix.g * 128.553 + pix.b * 24.966;
	float u = 128 - pix.r * 37.797 - pix.g * 74.203 + pix.b * 112.0;
	float v = 128 + pix.r * 112.0 - pix.g * 93.786 - pix.b * 18.214;

	ChannelY[pos.y * Strides[0] + pos.x] = round(y);
	ChannelU[pos.y * Strides[1] + pos.x] = round(u);
	ChannelV[pos.y * Strides[2] + pos.x] = round(v);
}
