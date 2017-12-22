#include <YUVShared.hlsl>
#include <Utility.hlsl>

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	int width = Dimensions.x;
	int height = Dimensions.y;
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(width, pos);
	float3 pix = WorkBuffer[index].Color;

	float y = Y_Full(pix);
	float u = U_Full(pix);
	float v = V_Full(pix);

	ChannelY[pos.y * Strides[0] + pos.x] = round(y);
	ChannelU[pos.y * Strides[1] + pos.x] = round(u);
	ChannelV[pos.y * Strides[2] + pos.x] = round(v);
}
