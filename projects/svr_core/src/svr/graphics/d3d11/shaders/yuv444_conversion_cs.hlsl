#include "cs_shared.hlsli"
#include "yuv_shared.hlsli"

Texture2D<float4> input_texture : register(t0);

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint> output_texture_u : register(u1);
RWTexture2D<uint> output_texture_v : register(u2);

[numthreads(cs_threads_x, cs_threads_y, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    // All planes are of equal size to the source material.

    float4 pix = input_texture.Load(dtid);

    // Input texture is in BGRA format.
    uint y = yuv_calc_y_444(pix.xyz);
    uint u = yuv_calc_u(pix.xyz);
    uint v = yuv_calc_v(pix.xyz);

    output_texture_y[dtid.xy] = y;
    output_texture_u[dtid.xy] = u;
    output_texture_v[dtid.xy] = v;
}
