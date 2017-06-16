#include "Common_PS.hlsl"

struct PS_OUTPUT
{
	float4 Color : SV_Target0;
};

Texture2D Frame : register(t0);

cbuffer InputData : register(b0)
{
	float Weight;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT ret;

	float4 framecol = float4(Frame.Sample(TextureSampler, input.Tex).rgb, 1.0);

	float4 mult = float4(Weight, Weight, Weight, 1);

	ret.Color = mult * framecol;

	return ret;
}
