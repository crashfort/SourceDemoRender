#include "PrecompiledHeader.hpp"
#include "HLAE.hpp"

/*
	The files in this directory are by Dominik 'ripieces' Tugend from HLAE
	See Readme

	https://github.com/ripieces/advancedfx
	https://github.com/ripieces/advancedfx/blob/master/resources/readme.txt
*/

int HLAE::CalcPitch(int width, unsigned char bytePerPixel, int byteAlignment)
{
	if (byteAlignment < 1)
	{
		return 0;
	}

	int pitch = width * (int)bytePerPixel;

	if (0 != pitch % byteAlignment)
	{
		pitch = (1 + (pitch / byteAlignment)) * byteAlignment;
	}

	return pitch;
}
