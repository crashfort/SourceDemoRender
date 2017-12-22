#include <Utility.hlsl>
#include <SharedAll.hlsl>

StructuredBuffer<WorkBufferData> WorkBufferSRV : register(t0);

struct PS_INPUT
{
	float4 Position : SV_POSITION0;
};

struct PS_OUTPUT
{
	float4 Color : SV_TARGET0;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT ret;

	uint index = CalculateIndex(Dimensions.x, int2(input.Position.xy));
	float3 pix = WorkBufferSRV[index].Color;

	ret.Color = float4(pix.rgb, 1);

	return ret;
}
