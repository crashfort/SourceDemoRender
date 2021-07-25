Texture2D<unorm float4> source_texture : register(t0);

// If the hardware does not support typed UAV loads for R32G32B32A32_FLOAT then we have to do more work.
// We need to use 32 bits per component in order to retain the accuracy of blending together multiple textures.
// If we don't have the hardware support for this texture format, we need to use a memory buffer.

#if MOTION_SAMPLE_LEGACY
RWStructuredBuffer<float4> dest_buffer : register(u0);
#else
RWTexture2D<float4> dest_texture : register(u0);
#endif

cbuffer mosample_buffer_0 : register(b0)
{
    float mosample_weight;
};

#if MOTION_SAMPLE_LEGACY
cbuffer mosample_buffer_1 : register(b1)
{
    uint dest_texture_width;
};
#endif

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 pos = dtid.xy;

    float4 source_pix = source_texture.Load(dtid);

    #if MOTION_SAMPLE_LEGACY
    uint di = pos.x + dest_texture_width * pos.y;
    dest_buffer[di] = source_pix * mosample_weight;
    #else
    dest_texture[pos] += source_pix * mosample_weight;
    #endif
}
