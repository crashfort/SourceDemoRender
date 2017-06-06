struct VS_INPUT
{
    uint ID : SV_VertexID;
};

struct VS_OUTPUT
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT ret;

    ret.UV = float2
	(
		input.ID & 1 ? 0 : 1,
		input.ID & 2 ? 1 : 0
	);

    ret.Position = float4(ret.UV, 0, 1);

    return ret;
}
