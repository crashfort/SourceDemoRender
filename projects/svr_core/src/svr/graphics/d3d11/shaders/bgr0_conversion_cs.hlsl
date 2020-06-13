#include "cs_shared.hlsli"

Texture2D<float4> input_texture : register(t0);
RWTexture2D<uint4> output_texture_bgr0 : register(u0);

[numthreads(cs_threads_x, cs_threads_y, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    // Identical data to the input source format.
    // This is required because D3D11 is unable to automatically convert between RGBA and BGRA.
    // Swizzle the components.

    float4 pix = input_texture.Load(dtid);
    output_texture_bgr0[dtid.xy] = float4(pix.zyx, 1) * 255.0;
}
