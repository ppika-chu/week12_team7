#pragma pack_matrix(row_major)

#include "ColorSpace.hlsli"

cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

// FParticleSpriteInstanceData와 같은 필드 순서
struct FParticleSpriteInstanceData
{
    float3 Center;
    float Rotation;
    float2 Size;
    float4 Color;
};

StructuredBuffer<FParticleSpriteInstanceData> ParticleInstances : register(t22);
Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float2 UV : TEXCOORD0;
    uint InstanceId : SV_InstanceID;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

PS_INPUT mainVS(VS_INPUT Input)
{
    const FParticleSpriteInstanceData Particle = ParticleInstances[Input.InstanceId];

	// 현재 엔진의 VP에서 카메라 오른쪽·위쪽 방향 복원
    const float3 CameraRight = normalize(float3(VP[0][0], VP[1][0], VP[2][0]));
    const float3 CameraUp = normalize(float3(VP[0][1], VP[1][1], VP[2][1]));

    float SinRotation;
    float CosRotation;
    sincos(Particle.Rotation, SinRotation, CosRotation);

    const float2 Offset = Input.Position.yz * Particle.Size;
    const float2 RotatedOffset = float2(
		Offset.x * CosRotation - Offset.y * SinRotation,
		Offset.x * SinRotation + Offset.y * CosRotation);

    const float3 WorldCenter = mul(float4(Particle.Center, 1.0f), World).xyz;
    const float3 WorldPosition = WorldCenter
		+ CameraRight * RotatedOffset.x + CameraUp * RotatedOffset.y;

    PS_INPUT Output;
    Output.Position = mul(float4(WorldPosition, 1.0f), VP);
    Output.UV = Input.UV;
    Output.Color = Particle.Color;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    const float4 Sample = SpriteTexture.Sample(SpriteSampler, Input.UV);
    return float4(SRGBToLinear(Sample.rgb) * Input.Color.rgb, Sample.a * Input.Color.a);
}