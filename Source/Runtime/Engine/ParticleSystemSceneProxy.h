#pragma once

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/Material.h"
#include "Render/VertexFactory.h"

class UParticleSystemComponent;
class FVertexBuffer;
class FIndexBuffer;
struct FParticleDynamicData;
struct FDynamicSpriteEmitterReplayDataBase;

// UParticleSystemComponent의 렌더 쪽 (UE ParticleSystemRender.cpp의 FParticleSystemSceneProxy)
// 컴포넌트가 Tick에서 dirty를 표시하면, 첫 가시 View에서 스냅샷을 받아 가진다.
// 이미터마다 인스턴스 버퍼를 하나 두고 View마다 다시 채워 드로우콜 1번으로 그린다.
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

private:
	// 이미터 하나를 그리는 GPU 자원. 패킷이 주소를 들고 있으므로 힙에 두어 위치를 고정한다.
	struct FEmitterRenderResources
	{
		TUniquePtr<FVertexBuffer> InstanceBuffer;
		uint32 MaxInstances = 0;
		FVertexFactory VertexFactory;
		FParticleSpriteUniformParameters Params;
	};

	// 정렬용 임시 항목 (UE FParticleOrder)
	struct FParticleOrder
	{
		int32 ParticleIndex = 0;
		float Z = 0.0f;   // 거리 정렬 키
		uint32 C = 0;     // 나이 정렬 키
	};

	FEmitterRenderResources& GetEmitterResources(int32 EmitterIndex) const;
	void SortSpriteParticles(const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, const FVector& CameraLocation) const;
	bool FillInstanceBuffer(FEmitterRenderResources& Resources, const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, bool bSorted) const;

	TUniquePtr<FParticleDynamicData> DynamicData;

	// 모든 이미터가 같이 쓰는 사각형 (UE GParticleTexCoordVertexBuffer, GParticleIndexBuffer)
	TUniquePtr<FVertexBuffer> QuadVertexBuffer;
	TUniquePtr<FIndexBuffer> QuadIndexBuffer;

	// View마다 다시 채우는 값이라 const 수집 함수에서도 바꾼다.
	mutable TArray<TUniquePtr<FEmitterRenderResources>> EmitterResources;
	mutable TArray<FParticleOrder> ParticleOrder;
};
