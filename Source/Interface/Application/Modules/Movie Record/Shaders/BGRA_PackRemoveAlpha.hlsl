#include "PackUtility.hlsl"

/*
	This shader converts a UNORM texture into a uint BGR0 format
*/

Texture2D<float4> RGB32Texture : register(t0);
RWBuffer<uint> OutTexture : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	uint2 pos = dtid.xy;
	float4 pix = RGB32Texture.Load(dtid);

	uint width;
	uint height;
	RGB32Texture.GetDimensions(width, height);

	uint4 color = uint4(round(pix.rgb * 255.0), 255);
	uint final = PackBGRA(color.b, color.g, color.r, 255);

	OutTexture[pos.y * (width) + pos.x] = final;
}
