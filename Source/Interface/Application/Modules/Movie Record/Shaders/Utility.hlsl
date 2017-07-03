inline uint CalculateIndex(int width, uint2 pos, int2 offset)
{
	pos += offset;
	return pos.y * (width) + pos.x;
}

inline uint CalculateIndex(int width, uint2 pos)
{
	return CalculateIndex(width, pos, int2(0, 0));
}
