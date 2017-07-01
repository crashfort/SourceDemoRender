#include "SharedAll.hlsl"

StructuredBuffer<float3> WorkBuffer : register(t0);
RWBuffer<uint> ChannelY : register(u0);
RWBuffer<uint> ChannelU : register(u1);
RWBuffer<uint> ChannelV : register(u2);

cbuffer YUVInputData : register(b1)
{
	int3 Strides;
};
