// Covers a full render target.

struct VS_OUTPUT
{
    float2 coord : TEXCOORD0;
    float4 pos : SV_POSITION0;
};

VS_OUTPUT main(uint id : SV_VERTEXID)
{
    VS_OUTPUT ret;
    ret.coord = float2(id % 2, id % 4 / 2);
    ret.pos = float4((ret.coord.x - 0.5) * 2.0, -(ret.coord.y - 0.5) * 2.0, 0, 1);

    return ret;
}
