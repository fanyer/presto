cbuffer constants : register (b0)
{
	float4x4 worldProjMatrix : packoffset (c0);
	float4 texTransS[2] : packoffset (c4);
	float4 texTransT[2] : packoffset (c6);
};

struct VS_INPUT
{
	float4 pos : TEXCOORD0;
	float2 texpos1 : TEXCOORD1;
	float4 color : TEXCOORD2;
};

struct VS_OUTPUT
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float2 texpos1 : TEXCOORD0;
	float2 texgenpos : TEXCOORD1;
	float2 texgenpos2 : TEXCOORD2;
};

VS_OUTPUT op_main(VS_INPUT v)
{
	VS_OUTPUT data;
	data.pos = mul(v.pos, worldProjMatrix);
	data.texpos1 = v.texpos1;
	data.texgenpos = float2(dot(texTransS[0], v.pos), dot(texTransT[0], v.pos));
	data.texgenpos2 = float2(dot(texTransS[1], v.pos), dot(texTransT[1], v.pos));
	data.color = v.color;
	return data;
}

