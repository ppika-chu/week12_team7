#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"

cbuffer ViewConstants : register(b0)
{
    matrix ViewProjection;
};

Texture2D BeamTexture : register(t0);
SamplerState BeamSampler : register(s0);

// FParticleBeamVertex의 월드 공간 정점 레이아웃.
struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    PS_INPUT Output;
    Output.Position = mul(float4(Input.Position, 1.0f), ViewProjection);
    Output.UV = Input.UV;
    Output.Color = Input.Color;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    const float4 Texel = BeamTexture.Sample(BeamSampler, Input.UV);
    return float4(SRGBToLinear(Texel.rgb) * Input.Color.rgb, Texel.a * Input.Color.a);
}
