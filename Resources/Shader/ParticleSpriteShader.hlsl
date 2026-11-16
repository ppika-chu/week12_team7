#pragma pack_matrix(row_major)
#include "ColorSpace.hlsli"

// 스프라이트 파티클 인스턴싱 셰이더 (UE ParticleSpriteVertexFactory.ush의 단순화)
// 0번 슬롯: 사각형 꼭짓점 4개(UV)
// 1번 슬롯: 파티클마다 FParticleSpriteVertex 하나 (INST_ 시맨틱)

// Renderer.h FViewConstants의 앞부분
cbuffer ViewConstants : register(b0)
{
    matrix ViewProjection;
    float3 CameraWorldPos;
    uint CameraPositionPadding;
    float3 CameraForward;
    uint DirectionalCascadeCount;
};

// Material.h FParticleSpriteUniformParameters와 같아야 한다.
cbuffer ParticleSpriteParams : register(b1)
{
    float4 SubImageSize;   // (가로 칸 수, 세로 칸 수, 1/가로, 1/세로)
    float2 PivotOffset;    // 꼭짓점 UV에 더해 중심을 원점으로 옮긴다. 기본 (-0.5, -0.5)
    uint ScreenAlignment;  // EParticleScreenAlignment
    float SpriteParamsPadding;
};

// Renderer.h FPerObjectConstants의 앞부분. 월드 공간 이미터는 단위 행렬, 로컬 공간이면 컴포넌트 행렬
cbuffer PerObjectConstants : register(b2)
{
    matrix World;
};

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

// 선언 순서와 크기가 FParticleSpriteVertex(64바이트)와 같아야 한다.
struct VS_INPUT
{
    float2 UV : TEXCOORD0;
    float3 Position : INST_POSITION;
    float RelativeTime : INST_RELATIVETIME;
    float3 OldPosition : INST_OLDPOSITION;
    float ParticleId : INST_PARTICLEID;
    float2 Size : INST_SIZE;
    float Rotation : INST_ROTATION;
    float SubImageIndex : INST_SUBIMAGEINDEX;
    float4 Color : INST_COLOR;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR0;
};

static const uint PSA_FacingCameraPosition = 0;
static const uint PSA_Velocity = 3;

// View 방향을 보는 평면의 오른쪽·위쪽 축. 결과의 cross(Right, Up)는 ViewDir과 같다.
void MakeFacingAxes(float3 ViewDir, out float3 Right, out float3 Up)
{
    const float3 RefUp = abs(ViewDir.z) > 0.999f ? float3(1, 0, 0) : float3(0, 0, 1);
    Right = normalize(cross(RefUp, ViewDir));
    Up = cross(ViewDir, Right);
}

PS_INPUT mainVS(VS_INPUT Input)
{
    const float3 WorldPos = mul(float4(Input.Position, 1.0f), World).xyz;

    float3 Right;
    float3 Up;
    if (ScreenAlignment == PSA_FacingCameraPosition)
    {
        // 파티클마다 카메라 위치를 향한다.
        MakeFacingAxes(normalize(WorldPos - CameraWorldPos), Right, Up);
    }
    else if (ScreenAlignment == PSA_Velocity)
    {
        // 위쪽 축을 이동 방향에 맞춘다. 멈춰 있으면 카메라 평면을 쓴다.
        const float3 WorldOld = mul(float4(Input.OldPosition, 1.0f), World).xyz;
        const float3 Move = WorldPos - WorldOld;
        const float3 ViewDir = normalize(WorldPos - CameraWorldPos);
        MakeFacingAxes(ViewDir, Right, Up);
        if (dot(Move, Move) > 1.0e-10f)
        {
            Up = normalize(Move);
            const float3 Side = cross(Up, ViewDir);
            if (dot(Side, Side) > 1.0e-10f)
            {
                Right = normalize(Side);
            }
        }
    }
    else
    {
        // PSA_Square, PSA_Rectangle: 모든 파티클이 카메라 평면과 나란하다.
        MakeFacingAxes(CameraForward, Right, Up);
    }

    // UV (0,0)이 왼쪽 위다. 중심 기준 오프셋으로 바꾸고 회전한다.
    const float2 Corner = (Input.UV + PivotOffset) * float2(1.0f, -1.0f) * Input.Size;
    float SinR;
    float CosR;
    sincos(Input.Rotation, SinR, CosR);
    const float2 Rotated = float2(Corner.x * CosR - Corner.y * SinR, Corner.x * SinR + Corner.y * CosR);

    PS_INPUT Output;
    Output.Position = mul(float4(WorldPos + Right * Rotated.x + Up * Rotated.y, 1.0f), ViewProjection);

    // SubImageIndex 칸의 UV. 1x1이면 텍스처 전체다.
    const float Index = floor(Input.SubImageIndex);
    const float Col = fmod(Index, SubImageSize.x);
    const float Row = floor(Index * SubImageSize.z);
    Output.UV = (Input.UV + float2(Col, Row)) * SubImageSize.zw;

    Output.Color = Input.Color;
    return Output;
}

float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    const float4 Texel = SpriteTexture.Sample(SpriteSampler, Input.UV);
    // 텍스처는 sRGB, 파티클 색은 선형이다. 선형으로 맞춘 뒤 곱한다.
    return float4(SRGBToLinear(Texel.rgb) * Input.Color.rgb, Texel.a * Input.Color.a);
}
