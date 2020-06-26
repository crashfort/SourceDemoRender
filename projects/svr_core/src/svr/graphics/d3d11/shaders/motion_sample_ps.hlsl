Texture2D<float4> input_texture : register(t0);
SamplerState input_texture_sampler : register(s0);

cbuffer motion_sample_buffer_0 : register(b0)
{
    float motion_sample_weight;
};

float4 main(float2 coord : TEXCOORD0) : SV_TARGET0
{
    float4 pix = input_texture.Sample(input_texture_sampler,coord);
    return float4(pix.rgb, 1) * motion_sample_weight;
}
