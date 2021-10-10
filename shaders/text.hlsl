#if TEXT_VS
struct VeloVtx
{
    float2 uv;
    float2 pos;
};

struct VS_OUTPUT
{
    float2 uv : TEXCOORD0;
    float4 pos : SV_POSITION0;
};

StructuredBuffer<VeloVtx> text_vtxs : register(t0);

VS_OUTPUT main(uint id : SV_VERTEXID)
{
    VS_OUTPUT ret;
    ret.uv = text_vtxs[id].uv;
    ret.pos = float4(text_vtxs[id].pos, 0, 1);

    return ret;
}

#elif TEXT_PS

Texture2D<float4> input_texture : register(t0);
SamplerState input_texture_sampler : register(s0);

float4 main(float2 coord : TEXCOORD0) : SV_TARGET0
{
    float4 pix = input_texture.Sample(input_texture_sampler, coord);

    // Useful for debugging.
    // pix.a = 0.5;

    return pix;
}

#endif
