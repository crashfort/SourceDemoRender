cbuffer mosample_buffer_0 : register(b0)
{
    float mosample_weight;
};

Texture2D<unorm float4> source_texture : register(t0);
RWTexture2D<float4> dest_texture : register(u0);

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    float4 source_pix = source_texture.Load(dtid);
    dest_texture[dtid.xy] += source_pix * mosample_weight;
}
