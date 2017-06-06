SamplerState TextureSampler
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};

struct PS_INPUT
{
    float4 Pos : SV_Position;
    float2 Tex : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
};

Texture2D NewSample : register(t0);

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT ret;

    ret.Color = float4(NewSample.Sample(TextureSampler, input.Tex).rgb, 1.0);

    return ret;
}
