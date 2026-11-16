#pragma once

#include "Engine/PrimitiveSceneProxy.h"
#include "Components/ParticleSystemComponent.h"
#include "Components/BillboardComponent.h"
#include "Render/VertexFactory.h"
#include <Math/Vector.h>
#include <Math/Vector2.h>
#include <Math/Vector4.h>

class FVertexBuffer;
class FIndexBuffer;
struct FParticleOrder;
struct FParticleSpriteVertex;
struct FDynamicSpriteEmitterReplayDataBase;

// GPU로 올리는 정점. 파티클 하나가 이 정점 4개(사각형)가 된다.
struct FParticleSpriteQuadVertex
{
	FVector Position; // 월드 좌표
	FVector2 UV;
	FVector4 Color;
};
static_assert(sizeof(FParticleSpriteQuadVertex) == 36, "ParticleSpriteShader.hlsl의 VS_INPUT과 크기가 같아야 한다");

class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* Component);
	~FParticleSystemSceneProxy() override;

	bool GatherDynamicRenderPackets(FRenderQueue& RenderQueue, FRenderQueue* IconQueue,
		const FVector& CameraLocation, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext,
		TArray<int32>& SortedParticleIndices, TArray<float>& ParticleDistancesSquared) const override;

protected:
	void UpdateDynamicData() override;
	void AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const override;

	// 정렬했으면 true, 정렬하지 않거나 지원하지 않는 모드면 false.
	bool SortSpriteParticles(int32 SortMode, bool bLocalSpace, int32 ParticleCount,
		const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices,
		const FVector& CameraLocation) const;

	void GetVertexAndIndexData(const FDynamicSpriteEmitterReplayDataBase& Source, const FParticleOrder* Order,
		int32 ParticleCount, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext) const;

private:
	void AddQuad(const FParticleSpriteVertex& Sprite, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext) const;
	void UploadVertices() const;
	void EnsureIndexBuffer(uint32 QuadCount) const;

	// =====================
	// mutable
	// 이번 프레임에 모든 뷰가 낸 정점. 뷰마다 뒤에 이어 붙여서, 먼저 수집한 뷰의 정점이 덮이지 않게 한다.
	mutable TArray<FParticleSpriteQuadVertex> Vertices;
	mutable TArray<FParticleOrder> ParticleOrder;

	mutable TUniquePtr<FVertexBuffer> VertexBuffer;
	mutable uint32 VertexCapacity = 0; // 정점 개수
	mutable TUniquePtr<FIndexBuffer> IndexBuffer;
	mutable uint32 IndexQuadCapacity = 0; // 사각형 개수

	// 크기를 키우며 교체된 옛 인덱스 버퍼. 이번 프레임의 패킷이 아직 가리킬 수 있어 다음 프레임에 지운다.
	mutable TArray<TUniquePtr<FIndexBuffer>> RetiredIndexBuffers;
	mutable FVertexFactory VertexFactory;

	// 이미터 전체에 공통인 머티리얼 상수. 파티클별 색은 정점에 들어간다.
	FSubUVConstants Constants;

	// 이번 프레임에 쓰는 머티리얼. 프록시가 참조 중임을 엔진에 알리는 데 쓴다.
	TArray<UMaterial*> Materials;
};
