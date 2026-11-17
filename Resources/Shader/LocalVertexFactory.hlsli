#include "VertexFactoryCommon.hlsli"

struct FVertexFactoryIntermediates
{
    float3 LocalPosition;
    float3x3 TangentToLocal;
    float TangentToWorldSign;
#if VF_MESH_PARTICLE
    float3x3 ParticleRotation;
#endif
};

#if VF_MESH_PARTICLE
float3x3 GetParticleRotation(float3 Rotation)
{
    float3 S, C;
    sincos(Rotation, S, C);
    const float3x3 X = float3x3(1, 0, 0, 0, C.x, S.x, 0, -S.x, C.x);
    const float3x3 Y = float3x3(C.y, 0, -S.y, 0, 1, 0, S.y, 0, C.y);
    const float3x3 Z = float3x3(C.z, S.z, 0, -S.z, C.z, 0, 0, 0, 1);
    // 행 벡터 기준 X → Y → Z, 단위는 라디안.
    return mul(mul(X, Y), Z);
}

float3x3 VertexFactoryGetParticleWorldAxes(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
    const float3x3 Axes = mul(Intermediates.ParticleRotation, (float3x3)World);
    return float3x3(Axes[0] * Input.InstanceScale.x, Axes[1] * Input.InstanceScale.y, Axes[2] * Input.InstanceScale.z);
}
#endif

float3x3 CalcTangentToLocal(FVertexFactoryInput Input, inout float TangentSign)
{
    const float3 TangentX = Input.Tangent.xyz;
    const float3 TangentZ = Input.Normal;
    TangentSign = Input.Tangent.w;
    const float3 TangentY = cross(TangentZ, TangentX) * TangentSign;
    return float3x3(TangentX, TangentY, TangentZ);
}

FVertexFactoryIntermediates GetVertexFactoryIntermediates(FVertexFactoryInput Input)
{
    FVertexFactoryIntermediates Intermediates;
    float TangentSign = 1.0f;
    Intermediates.LocalPosition = Input.Position;
    Intermediates.TangentToLocal = CalcTangentToLocal(Input, TangentSign);
    Intermediates.TangentToWorldSign = TangentSign * GetWorldDeterminantSign();
#if VF_MESH_PARTICLE
    Intermediates.ParticleRotation = GetParticleRotation(Input.InstanceRotation);
#endif
    return Intermediates;
}

float3 VertexFactoryGetLocalPosition(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
    return Intermediates.LocalPosition;
}

float4 VertexFactoryGetWorldPosition(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
#if VF_MESH_PARTICLE
    const float3 Position = mul(Intermediates.LocalPosition * Input.InstanceScale, Intermediates.ParticleRotation) + Input.InstancePosition;
    return mul(float4(Position, 1.0f), World);
#else
    return mul(float4(Intermediates.LocalPosition, 1.0f), World);
#endif
}

float3x3 VertexFactoryGetTangentToLocal(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
    return Intermediates.TangentToLocal;
}

float3 VertexFactoryGetWorldNormal(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
    float3 Normal = Intermediates.TangentToLocal[2];
#if VF_MESH_PARTICLE
    Normal = mul(Normal / max(Input.InstanceScale, 1e-6f), Intermediates.ParticleRotation);
#endif
    return mul(float4(Normal, 0.0f), WorldInverseTranspose).xyz;
}
