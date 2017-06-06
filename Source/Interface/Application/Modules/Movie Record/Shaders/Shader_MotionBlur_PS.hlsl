struct PS_INPUT
{
    float2 UV : TEXCOORD0;
};

struct PS_OUTPUT
{
    float4 Color : SV_Target0;
};

PS_OUTPUT PSMain(PS_INPUT input)
{
    PS_OUTPUT ret;

    ret.Color = float4(1, 1, 0, 1);

    return ret;
}
