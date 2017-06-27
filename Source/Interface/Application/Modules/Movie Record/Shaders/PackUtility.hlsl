uint PackBGRA(uint b, uint g, uint r, uint a)
{
	return ((a << 24) | (r << 16) | (g << 8) | (b)) & 0xFFFFFFFF;
}

void UnpackBGRA(uint input, out uint b, out uint g, out uint r, out uint a)
{
	b = (input >> 24) & 0x000000FF;
	g = (input >> 16) & 0x000000FF;
	r = (input >> 8) & 0x000000FF;
	a = (input) & 0x000000FF;
}
