#include "YUVShared.hlsl"
#include "Utility.hlsl"

[numthreads(ThreadsX, ThreadsY, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
	int width = Dimensions.x;
	int height = Dimensions.y;
	uint2 pos = dtid.xy;
	uint index = CalculateIndex(width, pos);

	float3 pix = WorkBuffer[index].Color;

	float Y = 16 + pix.r * 65.481 + pix.g * 128.553 + pix.b * 24.966;
	ChannelY[(height - pos.y - 1) * Strides[0] + pos.x] = round(Y);

	if ((pos.x & 1) == 0 && (pos.y & 1) == 0)
	{
		/*
			Average the 4 pixel values for better results
		*/
		float3 topright;
		float3 botleft;
		float3 botright;

		if (pos.x + 1 < width)
		{
			topright = WorkBuffer.Load(CalculateIndex(width, pos, int2(1, 0))).Color;
		}

		else
		{
			topright = pix;
		}

		if (pos.y + 1 < height)
		{
			botleft = WorkBuffer.Load(CalculateIndex(width, pos, int2(0, 1))).Color;
		}

		else 
		{
			botleft = pix;
		}

		if (pos.x + 1 < width && pos.y + 1 < height)
		{
			botright = WorkBuffer.Load(CalculateIndex(width, pos, int2(1, 1))).Color;
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
