#pragma once

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/Material.h"
#include "Render/VertexFactory.h"

class UParticleSystemComponent;
class FVertexBuffer;
class FIndexBuffer;
struct FParticleDynamicData;
struct FDynamicSpriteEmitterReplayDataBase;

// 이미터의 스냅샷과 뷰별 인스턴스 버퍼를 보관한다.
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

	// Cascade의 생성 순번 정렬: 작은 카운터가 먼저 태어난 파티클이다.
	struct FParticleOrder
	{
		int32 ParticleIndex = 0;
		float Z = 0.0f;
		uint32 C = 0;
	};
	void SortSpriteParticles(const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, const FVector& CameraLocation) const;
	mutable TArray<FParticleOrder> ParticleOrder;

private:
	// 패킷이 가리키는 주소와 각 뷰의 정렬 결과를 Capture가 끝날 때까지 유지한다.
	struct FEmitterRenderResources
	{
		TUniquePtr<FVertexBuffer> InstanceBuffer;
		uint32 MaxInstances = 0;
		FVertexFactory VertexFactory;
		FParticleSpriteUniformParameters Params;
	};

	FEmitterRenderResources& GetEmitterResources(int32 ResourceIndex) const;
	bool FillInstanceBuffer(FEmitterRenderResources& Resources, const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, bool bSorted) const;

	TUniquePtr<FParticleDynamicData> DynamicData;
	UMaterial* DefaultMaterial = nullptr;
	TUniquePtr<FVertexBuffer> QuadVertexBuffer;
	TUniquePtr<FIndexBuffer> QuadIndexBuffer;
	mutable TArray<TUniquePtr<FEmitterRenderResources>> EmitterResources;
	mutable int32 NextResourceIndex = 0;
};
