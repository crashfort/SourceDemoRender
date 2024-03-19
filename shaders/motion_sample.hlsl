cbuffer mosample_buffer_0 : register(b0)
{
    float mosample_weight;
};

Texture2D<unorm float4> source_texture : register(t0);
RWTexture2D<float4> dest_texture : register(u0);

float4 to_linear(float4 v)
{
    return pow(max(v, 0.0f), 2.2f);
}

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    // Must be blending in linear space! The mosample texture is high precision so this will not burn.
    float4 source_pix = to_linear(source_texture.Load(dtid));
    float4 new_pix = dest_texture[dtid.xy] + source_pix * mosample_weight;
    dest_texture[dtid.xy] = new_pix;
}
