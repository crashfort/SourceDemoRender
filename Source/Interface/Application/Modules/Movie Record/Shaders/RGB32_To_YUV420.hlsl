#include "YUVShared.hlsl"

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	int2 pos = dtid.xy;
	float4 pix = RGB32Texture.Load(dtid);

	uint width;
	uint height;
	RGB32Texture.GetDimensions(width, height);

	/*
		FFMpeg frames are flipped
	*/
	pos.y = height - pos.y - 1;

	float Y = 16 + pix.r * 65.481 + pix.g * 128.553 + pix.b * 24.966;
	ChannelY[(height - pos.y - 1) * Strides[0] + pos.x] = round(Y);

	if ((pos.x & 1) == 0 && (pos.y & 1) == 0)
	{
		/*
			Average the 4 pixel values for better results
		*/
		float4 topright;
		float4 botleft;
		float4 botright;

		if (pos.x + 1 < width)
		{
			topright = RGB32Texture.Load(dtid, int2(1, 0));
		}

		else
		{
			topright = pix;
		}

		if (pos.y + 1 < height)
		{
			botleft = RGB32Texture.Load(dtid, int2(0, 1));
		}

		else 
		{
			botleft = pix;
		}

		if (pos.x + 1 < width && pos.y + 1 < height)
		{
			botright = RGB32Texture.Load(dtid, int2(1, 1));
		}

		else 
		{
			botright = pix;
		}

		pix = (pix + topright + botleft + botright) / 4.0;

		pos.x >>= 1;
		pos.y >>= 1;

		pos.y = (height >> 1) - pos.y - 1;

		float U = 128 - pix.r * 37.797 - pix.g * 74.203 + pix.b * 112.0;
		float V = 128 + pix.r * 112.0 - pix.g * 93.786 - pix.b * 18.214;

		ChannelU[pos.y * Strides[1] + pos.x] = round(U);
		ChannelV[pos.y * Strides[2] + pos.x] = round(V);
	}
}
