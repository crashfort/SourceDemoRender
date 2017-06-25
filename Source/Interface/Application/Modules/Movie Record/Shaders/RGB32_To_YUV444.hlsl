Texture2D<float4> RGB32Texture : register(t0);
RWBuffer<uint> ChannelY : register(u0);
RWBuffer<uint> ChannelU : register(u1);
RWBuffer<uint> ChannelV : register(u2);

cbuffer InputData : register(b0)
{
	int3 Strides;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	int2 pos = dtid.xy;
	float4 pix = RGB32Texture.Load(dtid);

	float Y = 16 + pix.r * 65.481 + pix.g * 128.553 + pix.b * 24.966;
	float U = 128 - pix.r * 37.797 - pix.g * 74.203 + pix.b * 112.0;
	float V = 128 + pix.r * 112.0 - pix.g * 93.786 - pix.b * 18.214;

	ChannelY[pos.y * Strides[0] + pos.x] = round(Y);
	ChannelU[pos.y * Strides[1] + pos.x] = round(U);
	ChannelV[pos.y * Strides[2] + pos.x] = round(V);
}
