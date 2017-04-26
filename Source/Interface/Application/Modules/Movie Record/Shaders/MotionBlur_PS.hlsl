struct PS_OUTPUT
{
	float4 Color : SV_Target;
};

PS_OUTPUT PSMain() : SV_Target
{
	PS_OUTPUT ret;

	ret.Color = float4(1.0f, 1.0f, 1.0f, 1.0f);

	return ret;
}
