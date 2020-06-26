#include "cs_shared.hlsli"

Texture2D<unorm float4> source_texture : register(t0);
RWTexture2D<float4> dest_texture : register(u0);

cbuffer motion_sample_buffer_0 : register(b0)
{
    float motion_sample_weight;
};

[numthreads(cs_threads_x, cs_threads_y, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 pos = dtid.xy;
    float4 source_pix = source_texture.Load(dtid);

    dest_texture[pos] += source_pix * motion_sample_weight;
}
