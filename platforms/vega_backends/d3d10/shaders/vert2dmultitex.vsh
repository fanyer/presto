cbuffer constants : register (b0)
{
	float4x4 worldProjMatrix : packoffset (c0);
};

struct VS_INPUT
{
	float4 pos : TEXCOORD0;
	float2 texpos1 : TEXCOORD1;
	float2 texpos2 : TEXCOORD3;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float2 texpos1 : TEXCOORD0;
	float2 texpos2 : TEXCOORD1;
};

VS_OUTPUT op_main(VS_INPUT v)
{
	VS_OUTPUT data;
	data.pos = mul(v.pos, worldProjMatrix);
	data.texpos1 = v.texpos1;
	data.texpos2 = v.texpos2;
	data.color = float4(1.0,1.0,1.0,1.0);
	return data;
}

