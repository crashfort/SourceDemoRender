// Functions that can be used for converting rgb data to yuv.
// Will require that a constant buffer is bound to register 0.

cbuffer yuv_buffer_0 : register(b0)
{
    float4 coeff_y;
    float4 coeff_u;
    float4 coeff_v;
};

uint yuv_calc_y_444(float3 pix)
{
    pix *= 255.0;
    return 0 + (pix.x * coeff_y[0]) + (pix.y * coeff_y[1]) + (pix.z * coeff_y[2]);
}

uint yuv_calc_y_420(float3 pix)
{
    pix *= 255.0;
    return 16 + (pix.x * coeff_y[0]) + (pix.y * coeff_y[1]) + (pix.z * coeff_y[2]);
}

uint yuv_calc_u(float3 pix)
{
    pix *= 255.0;
    return 128 + (pix.x * coeff_u[0]) + (pix.y * coeff_u[1]) + (pix.z * coeff_u[2]);
}

uint yuv_calc_v(float3 pix)
{
    pix *= 255.0;
    return 128 + (pix.x * coeff_v[0]) + (pix.y * coeff_v[1]) + (pix.z * coeff_v[2]);
}

float4 yuv_average_nearby(uint3 dtid, float4 base, Texture2D<float4> tex)
{
    uint width;
    uint height;
    tex.GetDimensions(width, height);

    float4 topright;
    float4 botleft;
    float4 botright;

    if (dtid.x + 1 < width)
    {
        topright = tex.Load(dtid, int2(1, 0));
    }

    else
    {
        topright = base;
    }

    if (dtid.y + 1 < height)
    {
        botleft = tex.Load(dtid, int2(0, 1));
    }

    else
    {
        botleft = base;
    }

    if (dtid.x + 1 < width && dtid.y + 1 < height)
    {
        botright = tex.Load(dtid, int2(1, 1));
    }

    else
    {
        if (dtid.x + 1 < width)
        {
            botright = topright;
        }

        else if (dtid.y + 1 < height)
        {
            botright = botleft;
        }

        else
        {
            botright = base;
        }
    }

    return (base + topright + botleft + botright) / 4.0;
}
