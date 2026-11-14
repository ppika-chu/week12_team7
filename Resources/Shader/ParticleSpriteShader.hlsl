#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"

// Cascade 스프라이트 파티클용 셰이더.
// ParticleSubUVShader와 상수 버퍼는 같고, 정점마다 색을 받는 점만 다르다.
// 정점은 CPU에서 이미 카메라를 향하는 사각형으로 펼쳐진 월드 좌표다 (FParticleSpriteQuadVertex).

cbuffer constants : register(b0)
{
    matrix VP;
};

cbuffer Worldconstants : register(b2)
{
    matrix World;
};

// BillboardComponent.h의 FSubUVConstants와 바이트 단위로 같아야 한다.
cbuffer subuv : register(b1)
{
    float CurrentFrame;
    float AtlasRowSize;
    float AtlasColSize;
    float ParticleAlpha;
    float4 Tint;
    // 1: HDR 씬에 그리므로 sRGB -> 선형 변환, 0: 톤 매핑 이후 LDR이라 그대로
    float DecodeSRGB;
    float3 SubUVPadding;
};

Texture2D AtlasTexture : register(t0);
SamplerState AtlasSampler : register(s0);

// 순서와 크기가 FParticleSpriteQuadVertex와 같아야 한다. 입력 레이아웃은 이 선언에서 자동 생성된다.
struct VS_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(mul(float4(input.position, 1.0f), World), VP);
    output.uv = input.uv;
    output.color = input.color;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float atlasCol = fmod(CurrentFrame, AtlasColSize);
    float atlasRow = floor(CurrentFrame / AtlasColSize);

    float2 cellSize = float2(1.0f / AtlasColSize, 1.0f / AtlasRowSize);
    float2 cellUV = input.uv * cellSize + float2(atlasCol, atlasRow) * cellSize;

    float4 color = AtlasTexture.Sample(AtlasSampler, cellUV);
    color.a *= ParticleAlpha * input.color.a;
    if (DecodeSRGB > 0.5f)
    {
        color.rgb = SRGBToLinear(color.rgb) * SRGBToLinear(Tint.rgb);
    }
    else
    {
        color.rgb *= Tint.rgb;
    }

    // 파티클 색(FLinearColor)은 이미 선형 값이다.
    color.rgb *= input.color.rgb;

    return color;
}
