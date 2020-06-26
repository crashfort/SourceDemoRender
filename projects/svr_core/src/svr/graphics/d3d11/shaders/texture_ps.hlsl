Texture2D<float4> input_texture : register(t0);
SamplerState input_texture_sampler : register(s0);

float4 main(float2 coord : TEXCOORD0) : SV_TARGET0
{
    return input_texture.Sample(input_texture_sampler, coord);
}
