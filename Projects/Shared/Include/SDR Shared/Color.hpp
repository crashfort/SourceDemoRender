#pragma once
#include <cstdint>

namespace SDR::Shared
{
	struct Color
	{
		Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : Colors{ r, g, b, a }
		{

		}

		uint8_t Colors[4];
	};
}
