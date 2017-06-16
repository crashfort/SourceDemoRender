struct PS_INPUT
{
	float4 Pos : SV_Position;
	float2 Tex : TEXCOORD0;
};

SamplerState TextureSampler : register(s0);
