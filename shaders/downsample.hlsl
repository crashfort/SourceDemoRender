// Downsample from 128 bpp to 32 bpp.

Texture2D<float4> source_texture : register(t0);
RWTexture2D<unorm float4> dest_texture : register(u0);

float4 from_linear(float4 v)
{
    return pow(max(v, 0.0f), 1.0f / 2.2f);
}

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 pos = dtid.xy;
    float4 source_pix = from_linear(source_texture.Load(dtid)); // Must go back from linear space used in mosample.
    dest_texture[pos] = source_pix;
}
