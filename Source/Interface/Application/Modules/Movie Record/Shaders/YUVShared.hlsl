Texture2D<float4> RGB32Texture : register(t0);
RWBuffer<uint> ChannelY : register(u0);
RWBuffer<uint> ChannelU : register(u1);
RWBuffer<uint> ChannelV : register(u2);

cbuffer InputData : register(b0)
{
	int3 Strides;
};
