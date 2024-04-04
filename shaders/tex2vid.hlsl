// This file is intended to be compiled into many resulting shaders.
// Purpose of these are to convert the program pixel format to a video pixel format.

#define AVCOL_SPC_BT709 1
// #define AVCOL_SPC_BT470BG 1

// --------------------------------------------------------------------------------------------------------------------

// Output texture is based in UINT (0 to 255).
// Input texture based in BGRA unorm format (0.0 to 1.0).
Texture2D<float4> input_texture : register(t0);

// --------------------------------------------------------------------------------------------------------------------

uint3 convert_rgb_to_yuv(float3 rgb)
{
    rgb = rgb * 255.0f;

    // For meme reasons you appear to need to divide by 255.0 / 219.0.
    // This number comes from the partial MPEG range. We don't add 16.0 / 255.0 to this.
    rgb /= 1.164383;

    uint3 ret;

    #if AVCOL_SPC_BT709
    ret.x = 16  + (rgb.x * +0.212600) + (rgb.y * +0.715200) + (rgb.z * +0.072200);
    ret.y = 128 + (rgb.x * -0.114572) + (rgb.y * -0.385428) + (rgb.z * +0.500000);
    ret.z = 128 + (rgb.x * +0.500000) + (rgb.y * -0.454153) + (rgb.z * -0.045847);
    #elif AVCOL_SPC_BT470BG
    ret.x = 16  + (rgb.x * +0.299000) + (rgb.y * +0.587000) + (rgb.z * +0.114000);
    ret.y = 128 + (rgb.x * -0.168736) + (rgb.y * -0.331264) + (rgb.z * +0.500000);
    ret.z = 128 + (rgb.x * +0.500000) + (rgb.y * -0.418688) + (rgb.z * -0.081312);
    #endif

    return ret;
}

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_NV12
// Used by H264.

// The first plane is as large as the source material.
// The second plane is half the size and has U and V interleaved.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint2> output_texture_uv : register(u1);

void proc(uint3 dtid)
{
    float4 pix = input_texture.Load(dtid);
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_uv[dtid.xy >> 1] = uint2(yuv.yz);
}

#endif

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_YUV422P

// The first plane is as large as the source material.
// The second and third planes are half in size horizontally.
// Used by dnxhr.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint> output_texture_u : register(u1);
RWTexture2D<uint> output_texture_v : register(u2);

void proc(uint3 dtid)
{
    float4 pix = input_texture.Load(dtid);
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_u[int2(dtid.x >> 1, dtid.y)] = yuv.y;
    output_texture_v[int2(dtid.x >> 1, dtid.y)] = yuv.z;
}

#endif

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_YUV444P

// Every plane is as big as the source material.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint> output_texture_u : register(u1);
RWTexture2D<uint> output_texture_v : register(u2);

void proc(uint3 dtid)
{
    float4 pix = input_texture.Load(dtid);
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_u[dtid.xy] = yuv.y;
    output_texture_v[dtid.xy] = yuv.z;
}

#endif

// --------------------------------------------------------------------------------------------------------------------

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    proc(dtid);
}
