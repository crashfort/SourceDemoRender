#include "SharedAll.hlsl"

StructuredBuffer<WorkBufferData> WorkBuffer : register(t0);
RWBuffer<uint> ChannelY : register(u0);
RWBuffer<uint> ChannelU : register(u1);
RWBuffer<uint> ChannelV : register(u2);

cbuffer YUVInputData : register(b1)
{
	int3 Strides;
	int Padding1;
	float3 CoeffY;
	int Padding2;
	float3 CoeffU;
	int Padding3;
	float3 CoeffV;
};

float Y_Full(float3 pix)
{
	pix *= 255.0;
	return 0 + (pix.r * CoeffY[0]) + (pix.g * CoeffY[1]) + (pix.b * CoeffY[2]);
}

float U_Full(float3 pix)
{
	pix *= 255.0;
	return 128 + (pix.r * CoeffU[0]) + (pix.g * CoeffU[1]) + (pix.b * CoeffU[2]);
}

float V_Full(float3 pix)
{
	pix *= 255.0;
	return 128 + (pix.r * CoeffV[0]) + (pix.g * CoeffV[1]) + (pix.b * CoeffV[2]);
}
