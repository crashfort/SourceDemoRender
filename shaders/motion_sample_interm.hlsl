Texture2D<float4> source_texture : register(t0);
RWTexture2D<unorm float4> dest_texture : register(u0);

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint2 pos = dtid.xy;
    float4 source_pix = source_texture.Load(dtid);
    dest_texture[pos] = source_pix;
}
