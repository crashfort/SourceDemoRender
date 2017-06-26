/*
	This shader converts a UNORM texture into a uint BGR0 format
*/

Texture2D<float4> RGB32Texture : register(t0);
RWBuffer<uint> OutTexture : register(u0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	int2 pos = dtid.xy;
	float4 pix = RGB32Texture.Load(dtid);

	uint width;
	uint height;
	RGB32Texture.GetDimensions(width, height);

	uint4 color = uint4(round(pix.rgb * 255.0), 255);
	uint final = ((color.a << 24) | (color.r << 16) | (color.g << 8) | (color.b)) & 0xFFFFFFFF;

	OutTexture[pos.y * (width) + pos.x] = final;
}
