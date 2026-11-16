#pragma once

#include "PrimitiveSceneProxy.h"
#include "Particles/ParticleReplayData.h"
#include "Render/Buffer.h"
#include "Render/VertexFactory.h"

// 파티클 프레임 사본과 이미터별 GPU 자원 관리
class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UPrimitiveComponent* InComponent);

	// Capture 시작 전 프레임 사본 전달
	void SetDynamicData(TArray<FDynamicSpriteEmitterReplayData>&& InReplayData);
	bool GatherDynamicRenderPackets(FRenderQueue& RenderQueue, FRenderQueue* IconQueue,
		const FVector& CameraLocation, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext,
		TArray<int32>& SortedParticleIndices, TArray<float>& ParticleDistancesSquared) const override;
	void AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const override;

protected:
	void UpdateDynamicData() override;

private:
	// 이미터별 GPU 자원과 바인딩 상태
	struct FEmitterRenderData
	{
		TUniquePtr<FStructuredBuffer> InstanceBuffer;
		FVertexFactory VertexFactory;
		UMaterial* Material = nullptr;
		uint32 InstanceCount = 0;
	};

	UStaticMesh* QuadMesh = nullptr;
	UMaterial* DefaultMaterial = nullptr;
	TArray<FDynamicSpriteEmitterReplayData> ReplayData;
	TArray<FEmitterRenderData> RenderEmitters;
};