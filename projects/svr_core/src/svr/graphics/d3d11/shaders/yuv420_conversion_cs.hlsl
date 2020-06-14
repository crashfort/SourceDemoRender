#include "cs_shared.hlsli"
#include "yuv_shared.hlsli"

Texture2D<float4> input_texture : register(t0);

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint> output_texture_u : register(u1);
RWTexture2D<uint> output_texture_v : register(u2);

[numthreads(cs_threads_x, cs_threads_y, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    // The first plane is as large as the source material.
    // The second and third planes are half in width and height.

    float4 pix = input_texture.Load(dtid);
    float4 nearby = yuv_average_nearby(dtid, pix, input_texture);

    // Input texture is in BGRA format.
    uint y = yuv_calc_y(pix.xyz);
    uint u = yuv_calc_u(nearby.xyz);
    uint v = yuv_calc_v(nearby.xyz);

    output_texture_y[dtid.xy] = y;

    // Squeeze to half resolution in both dimensions.
    dtid.xy >>= 1;

    output_texture_u[dtid.xy] = u;
    output_texture_v[dtid.xy] = v;
}
