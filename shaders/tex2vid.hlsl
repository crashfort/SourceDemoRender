// This file is intended to be compiled into many resulting shaders.
// Purpose of these are to convert the program pixel format to a video pixel format.

// For the 4:2:0 formats we should ideally sample 2 pixels for the U and V planes, but that would mean
// adding branches to the shaders which we want to avoid. The difference is also so minimal that it doesn't matter when we will be
// using constantly moving content.

// --------------------------------------------------------------------------------------------------------------------

// Output texture is based in UINT (0 to 255).
// Input texture based in BGRA unorm format (0.0 to 1.0).
Texture2D<float4> input_texture : register(t0);

// --------------------------------------------------------------------------------------------------------------------

uint3 convert_rgb_to_yuv(float3 rgb)
{
    rgb = rgb * 255.0f;

    uint3 ret = uint3(0, 0, 0);

    #if AVCOL_SPC_BT709
    ret.x = 16  + (rgb.x * +0.212600) + (rgb.y * +0.715200) + (rgb.z * +0.072200);
    ret.y = 128 + (rgb.x * -0.114572) + (rgb.y * -0.385428) + (rgb.z * +0.500000);
    ret.z = 128 + (rgb.x * +0.500000) + (rgb.y * -0.454153) + (rgb.z * -0.045847);
    #elif AVCOL_SPC_BT470BG
    ret.x = 16  + (rgb.x * +0.299000) + (rgb.y * +0.587000) + (rgb.z * +0.114000);
    ret.y = 128 + (rgb.x * -0.168736) + (rgb.y * -0.331264) + (rgb.z * +0.500000);
    ret.z = 128 + (rgb.x * +0.500000) + (rgb.y * -0.418688) + (rgb.z * -0.081312);
    #endif

    // Resolve partial range by adding 16.0 / 255.0 and dividing by 255.0 / 219.0.
    // We don't do this as we don't specify a color range.
    #if 0
    rgb += 0.062745;
    rgb /= 1.164383;
    #endif

    return ret;
}

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_YUV420P

// The first plane is as large as the source material.
// The second and third planes are half in size.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint> output_texture_u : register(u1);
RWTexture2D<uint> output_texture_v : register(u2);

void proc(uint3 dtid)
{
    float4 pix = input_texture[dtid.xy];
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_u[dtid.xy >> 1] = yuv.y;
    output_texture_v[dtid.xy >> 1] = yuv.z;
}

#endif

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_YUV444P

// All planes are of equal size to the source material.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint> output_texture_u : register(u1);
RWTexture2D<uint> output_texture_v : register(u2);

void proc(uint3 dtid)
{
    float4 pix = input_texture[dtid.xy];
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_u[dtid.xy] = yuv.y;
    output_texture_v[dtid.xy] = yuv.z;
}

#endif

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_NV12

// The first plane is as large as the source material.
// The second plane is half the size and has U and V interleaved.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint2> output_texture_uv : register(u1);

void proc(uint3 dtid)
{
    float4 pix = input_texture[dtid.xy];
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_uv[dtid.xy >> 1] = uint2(yuv.yz);
}

#endif

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_NV21

// The first plane is as large as the source material.
// The second plane is half the size and has U and V interleaved.
// This is different from NV12 in that U and V are switched.

RWTexture2D<uint> output_texture_y : register(u0);
RWTexture2D<uint2> output_texture_vu : register(u1);

void proc(uint3 dtid)
{
    float4 pix = input_texture[dtid.xy];
    uint3 yuv = convert_rgb_to_yuv(pix.xyz);
    output_texture_y[dtid.xy] = yuv.x;
    output_texture_vu[dtid.xy >> 1] = uint2(yuv.zy);
}

#endif

// --------------------------------------------------------------------------------------------------------------------

#if AV_PIX_FMT_BGR0

// Data is packed in BGRX in a single plane. We have to swizzle it around and also multiply by 255.

RWTexture2D<uint4> output_texture_rgb : register(u0);

void proc(uint3 dtid)
{
    float4 pix = input_texture.Load(dtid);
    output_texture_rgb[dtid.xy] = uint4(float4(pix.zyx, 1) * 255.0);
}

#endif

// --------------------------------------------------------------------------------------------------------------------

// This must be synchronized with the compute shader Dispatch call in CPU code!
[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    proc(dtid);
}
