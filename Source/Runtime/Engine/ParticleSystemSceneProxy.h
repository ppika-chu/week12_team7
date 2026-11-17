#pragma once

#include "Engine/PrimitiveSceneProxy.h"
#include "Render/Material.h"
#include "Render/VertexFactory.h"

class UParticleSystemComponent;
class FVertexBuffer;
class FIndexBuffer;
struct FParticleDynamicData;
struct FDynamicEmitterReplayDataBase;
struct FDynamicBeamEmitterReplayDataBase;
struct FDynamicMeshEmitterReplayDataBase;
struct FDynamicRibbonEmitterReplayDataBase;
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
	void SortParticles(const FDynamicEmitterReplayDataBase& Source, bool bUseLocalSpace, const FVector& CameraLocation,
		const FVector& WorldSortOffset = FVector::ZeroVector) const;
	mutable TArray<FParticleOrder> ParticleOrder;

private:
	// 패킷이 가리키는 주소와 각 뷰의 정렬 결과를 Capture가 끝날 때까지 유지한다.
	struct FEmitterRenderResources
	{
		TUniquePtr<FVertexBuffer> DynamicVertexBuffer;
		TUniquePtr<FIndexBuffer> BeamIndexBuffer;
		TUniquePtr<FIndexBuffer> RibbonIndexBuffer;
		uint32 MaxVertices = 0;
		FVertexFactory VertexFactory;
		FParticleSpriteUniformParameters Params;
	};

	struct FRibbonPoint
	{
		FVector Position;
		FLinearColor Color;
		float DistanceAlongRibbon;
	};

	FEmitterRenderResources& GetEmitterResources(int32 ResourceIndex) const;
	static bool EnsureDynamicVertexBuffer(FEmitterRenderResources& Resources, uint32 VertexCount, uint32 Stride);
	static bool EnsureBeamIndexBuffer(FEmitterRenderResources& Resources, uint32 BeamCount);
	bool FillInstanceBuffer(FEmitterRenderResources& Resources, const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, bool bSorted) const;
	void GatherMeshRenderPackets(FRenderQueue& RenderQueue, const FDynamicMeshEmitterReplayDataBase& Source,
		const FVector& CameraLocation, float DistanceSquared) const;
	void GatherBeamRenderPackets(FRenderQueue& RenderQueue, const FDynamicBeamEmitterReplayDataBase& Source,
		const FVector& CameraLocation, float DistanceSquared) const;
	void GatherRibbonRenderPackets(FRenderQueue& RenderQueue, const FDynamicRibbonEmitterReplayDataBase& Source,
		const FVector& CameraLocation, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext, float DistanceSquared) const;
	UMaterial* GetBeamMaterial(UMaterial* Material) const;

	TUniquePtr<FParticleDynamicData> DynamicData;
	UMaterial* DefaultMaterial = nullptr;
	UMaterial* DefaultBeamMaterial = nullptr;
	TUniquePtr<FVertexBuffer> QuadVertexBuffer;
	TUniquePtr<FIndexBuffer> QuadIndexBuffer;
	mutable TArray<TUniquePtr<FEmitterRenderResources>> EmitterResources;
	mutable TArray<FRibbonPoint> RibbonPoints;
	mutable int32 NextResourceIndex = 0;
};
