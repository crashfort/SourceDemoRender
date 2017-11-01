#include "SharedAll.hlsl"

StructuredBuffer<WorkBufferData> WorkBuffer : register(t0);
RWBuffer<uint> ChannelY : register(u0);
RWBuffer<uint> ChannelU : register(u1);
RWBuffer<uint> ChannelV : register(u2);

cbuffer YUVInputData : register(b1)
{
	int4 Strides;
	float3x3 Coeffs;
};

float Y_Full(float3 pix)
{
	pix *= 255.0;
	return 0 + (pix.r * Coeffs[0][0]) + (pix.g * Coeffs[0][1]) + (pix.b * Coeffs[0][2]);
}

float U_Full(float3 pix)
{
	pix *= 255.0;
	return 128 + (pix.r * Coeffs[1][0]) + (pix.g * Coeffs[1][1]) + (pix.b * Coeffs[1][2]);
}

float V_Full(float3 pix)
{
	pix *= 255.0;
	return 128 + (pix.r * Coeffs[2][0]) + (pix.g * Coeffs[2][1]) + (pix.b * Coeffs[2][2]);
}
