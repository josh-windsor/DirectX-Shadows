cbuffer CommonApp
{
	float4x4 g_WVP;
}

struct VSInput
{
	float4 pos:POSITION;
};

struct PSInput
{
	float4 pos:SV_Position;
};

struct PSOutput
{
	float4 colour:SV_Target;
};

void VSMain(const VSInput input, out PSInput output)
{
	// Transform each vertex into world space
	output.pos = mul(input.pos, g_WVP);
}

void PSMain(const PSInput input, out PSOutput output)
{
	// Draw a pixel colour other than black with opacity on w
	output.colour = float4(1, 0, 0, 0.3f);
}
