#include "Common_PS.hlsl"

struct PS_OUTPUT
{
	float4 Color : SV_Target0;
};

Texture2D NewTexture : register(t0);

PS_OUTPUT PSMain(PS_INPUT input)
{
	PS_OUTPUT ret;
	
	ret.Color = float4(NewTexture.Sample(TextureSampler, input.Tex).rgb, 1.0);
	
	return ret;
}
