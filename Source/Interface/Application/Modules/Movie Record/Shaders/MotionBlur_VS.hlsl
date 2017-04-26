struct VS_INPUT
{
	float4 Position : POSITION;
};

float4 VSMain(VS_INPUT input) : SV_Position
{
	return input.Position;
}
