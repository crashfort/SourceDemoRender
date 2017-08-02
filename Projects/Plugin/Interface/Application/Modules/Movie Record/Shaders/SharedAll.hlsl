cbuffer SharedInputData : register(b0)
{
	int2 Dimensions;
};

/*
	Padding is required beacuse structured buffers are tightly packed.
*/
struct WorkBufferData
{
	float3 Color;
	float Padding;
};

#define ThreadsX 8
#define ThreadsY 8
