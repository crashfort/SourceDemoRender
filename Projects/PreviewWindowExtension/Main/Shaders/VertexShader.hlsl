struct VS_OUTPUT
{
	float4 Position : SV_POSITION0;
	float2 TextureCoordinate : TEXCOORD0;
};

VS_OUTPUT VSMain(uint id : SV_VERTEXID)
{
	VS_OUTPUT ret;
	ret.TextureCoordinate = float2(id % 2, id % 4 / 2);
	ret.Position = float4((ret.TextureCoordinate.x - 0.5f) * 2, -(ret.TextureCoordinate.y - 0.5f) * 2, 0, 1);
	
	return ret;
}
