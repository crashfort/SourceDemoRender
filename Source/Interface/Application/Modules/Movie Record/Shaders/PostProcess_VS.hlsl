struct VS_INPUT
{
	uint ID : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 Pos : SV_Position;
	float2 Tex : TEXCOORD0;
};

VS_OUTPUT VSMain(VS_INPUT input)
{
	VS_OUTPUT ret;

	ret.Tex = float2((input.ID << 1) & 2, input.ID & 2);
	ret.Pos = float4(ret.Tex * float2(2, -2) + float2(-1, 1), 0, 1);

	return ret;
}
