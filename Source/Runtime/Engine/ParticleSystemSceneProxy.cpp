#include "EnginePCH.h"
#include "Engine/ParticleSystemSceneProxy.h"

#include <algorithm>
#include <cmath>

#include "Asset/AssetManager.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleModuleRequired.h"
#include "Render/Buffer.h"
#include "Render/RenderCommand.h"
#include "Render/RenderPacket.h"

namespace
{
// 사각형 꼭짓점의 UV. (0,0)이 왼쪽 위
const FVector2 GQuadTexCoords[4] = {
	FVector2(0.0f, 0.0f),
	FVector2(1.0f, 0.0f),
	FVector2(1.0f, 1.0f),
	FVector2(0.0f, 1.0f),
};
// 화면에서 시계 방향 (왼쪽 위 → 오른쪽 위 → 오른쪽 아래)
const uint32 GQuadIndices[6] = { 0, 1, 2, 0, 2, 3 };

// UE GetParticleSize: 정사각형 계열 정렬은 X 크기를 Y에도 쓴다.
FVector2 GetParticleSize(const FBaseParticle& Particle, const FDynamicSpriteEmitterReplayDataBase& Source)
{
	FVector2 Size(std::abs(Particle.Size.X * Source.Scale.X), std::abs(Particle.Size.Y * Source.Scale.Y));
	if (Source.ScreenAlignment == PSA_Square
		|| Source.ScreenAlignment == PSA_FacingCameraPosition
		|| Source.ScreenAlignment == PSA_FacingCameraDistanceBlend)
	{
		Size.Y = Size.X;
	}
	return Size;
}
}

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, DefaultMaterial(UAssetManager::GetAssetByPath<UMaterial>("ParticleSpriteMaterial"))
{
	QuadVertexBuffer = RenderCommand::CreateStaticVertexBuffer(GQuadTexCoords, sizeof(GQuadTexCoords), sizeof(FVector2));
	QuadIndexBuffer = RenderCommand::CreateStaticIndexBuffer(GQuadIndices, 6);
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy() = default;

// UE는 컴포넌트가 스냅샷을 만들어 프록시로 보낸다. 여기서는 엔진 방식대로 프록시가 가져온다.
void FParticleSystemSceneProxy::UpdateDynamicData()
{
	// PrepareDynamicData는 dirty 여부와 관계없이 Capture마다 한 번 호출된다.
	NextResourceIndex = 0;
	if (!bRenderDynamicDataDirty)
	{
		return;
	}
	UParticleSystemComponent* ParticleComponent = static_cast<UParticleSystemComponent*>(GetComponent());
	TranslucencySortPriority = ParticleComponent->GetTranslucencySortPriority();
	DynamicData.reset(ParticleComponent->CreateDynamicData());
	bRenderDynamicDataDirty = false;
}

void FParticleSystemSceneProxy::AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const
{
	FPrimitiveSceneProxy::AddReferencedMaterials(OutMaterials);
	OutMaterials.Add(DefaultMaterial);
	if (!DynamicData)
	{
		return;
	}
	for (const FDynamicEmitterDataBase* EmitterData : DynamicData->DynamicEmitterDataArray)
	{
		if (EmitterData && EmitterData->GetSource().eEmitterType == DET_Sprite)
		{
			if (UMaterial* Material = static_cast<const FDynamicSpriteEmitterDataBase*>(EmitterData)->GetSourceData()->MaterialInterface)
			{
				OutMaterials.Add(Material);
			}
		}
	}
}

FParticleSystemSceneProxy::FEmitterRenderResources& FParticleSystemSceneProxy::GetEmitterResources(int32 EmitterIndex) const
{
	while (EmitterResources.Num() <= EmitterIndex)
	{
		TUniquePtr<FEmitterRenderResources> NewResources = MakeUnique<FEmitterRenderResources>();
		NewResources->VertexFactory.VertexBuffer = QuadVertexBuffer.get();
		EmitterResources.Add(std::move(NewResources));
	}
	return *EmitterResources[EmitterIndex];
}

// UE FDynamicSpriteEmitterDataBase::SortSpriteParticles. 결과는 ParticleOrder에 남는다.
void FParticleSystemSceneProxy::SortSpriteParticles(const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, const FVector& CameraLocation) const
{
	const FParticleDataContainer& Container = Source.DataContainer;
	const FMatrix& LocalToWorld = GetLocalToWorld();
	ParticleOrder.SetNum(ParticleCount);

	if (Source.SortMode == PSORTMODE_ViewProjDepth || Source.SortMode == PSORTMODE_DistanceToView)
	{
		// ViewProjDepth는 UE에서 클립 공간 W(카메라 앞 방향 깊이)지만, 수집 함수가 카메라 위치만 받으므로 거리로 대신한다.
		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[Index]);
			const FVector Position = Source.bUseLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
			ParticleOrder[Index].ParticleIndex = Index;
			ParticleOrder[Index].Z = (CameraLocation - Position).SizeSquared();
		}
		// 먼 것부터 그린다.
		std::sort(ParticleOrder.begin(), ParticleOrder.end(),
			[](const FParticleOrder& A, const FParticleOrder& B) { return A.Z > B.Z; });
	}
	else
	{
		const bool bOldestFirst = Source.SortMode == PSORTMODE_Age_OldestFirst;
		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[Index]);
			const uint32 Flags = static_cast<uint32>(Particle.Flags);
			ParticleOrder[Index].ParticleIndex = Index;
			ParticleOrder[Index].C = Flags & STATE_CounterMask;
		}
		// Cascade의 수정된 의미를 유지한다.
		std::sort(ParticleOrder.begin(), ParticleOrder.end(),
			[bOldestFirst](const FParticleOrder& A, const FParticleOrder& B)
			{
				return bOldestFirst ? A.C < B.C : A.C > B.C;
			});
	}
}

// UE FDynamicSpriteEmitterData::GetVertexAndIndexData. 파티클마다 FParticleSpriteVertex 하나를 쓴다.
bool FParticleSystemSceneProxy::FillInstanceBuffer(FEmitterRenderResources& Resources, const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, bool bSorted) const
{
	if (!Resources.InstanceBuffer || !Resources.InstanceBuffer->GetBuffer()
		|| Resources.MaxInstances < static_cast<uint32>(ParticleCount))
	{
		// 모자라면 두 배로 늘려 다시 만든다. 줄이지는 않는다.
		const uint32 NewMax = std::max<uint32>(static_cast<uint32>(ParticleCount), std::max<uint32>(Resources.MaxInstances * 2, 64));
		auto NewBuffer = RenderCommand::CreateDynamicVertexBuffer(NewMax * sizeof(FParticleSpriteVertex), sizeof(FParticleSpriteVertex));
		if (!NewBuffer || !NewBuffer->GetBuffer())
		{
			return false;
		}
		Resources.InstanceBuffer = std::move(NewBuffer);
		Resources.MaxInstances = NewMax;
		Resources.VertexFactory.InstanceBuffer = Resources.InstanceBuffer.get();
	}
	if (!Resources.InstanceBuffer || !Resources.InstanceBuffer->GetBuffer())
	{
		return false;
	}

	FParticleSpriteVertex* Dest = static_cast<FParticleSpriteVertex*>(RenderCommand::MapWriteDiscard(Resources.InstanceBuffer.get()));
	if (!Dest)
	{
		return false;
	}

	const FParticleDataContainer& Container = Source.DataContainer;
	const int64 FrameCount = static_cast<int64>(std::max(Source.SubImages_Horizontal, 1)) * std::max(Source.SubImages_Vertical, 1);
	const float MaxSubImageIndex = static_cast<float>(FrameCount - 1);
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		// 인스턴스 순서가 곧 그리는 순서다.
		const int32 ParticleIndex = bSorted ? ParticleOrder[Index].ParticleIndex : Index;
		const uint8* ParticleBase = Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[ParticleIndex];
		DECLARE_PARTICLE_CONST(Particle, ParticleBase);

		FParticleSpriteVertex Vertex;
		Vertex.Position = Particle.Location;
		Vertex.RelativeTime = Particle.RelativeTime;
		Vertex.OldPosition = Particle.OldLocation;
		Vertex.ParticleId = static_cast<float>(static_cast<uint32>(Particle.Flags) & STATE_CounterMask) / 10000.0f;
		Vertex.Size = GetParticleSize(Particle, Source);
		Vertex.Rotation = Particle.Rotation;
		Vertex.SubImageIndex = 0.0f; // SubUV 모듈이 생기면 SubUVDataOffset의 payload에서 읽는다
		if (Source.SubUVDataOffset > 0)
		{
			const float SubImageIndex = *reinterpret_cast<const float*>(ParticleBase + Source.SubUVDataOffset);
			if (std::isfinite(SubImageIndex))
			{
				Vertex.SubImageIndex = std::clamp(SubImageIndex, 0.0f, MaxSubImageIndex);
			}
		}
		Vertex.Color = Particle.Color;

		// 매핑된 메모리는 앞에서부터 쓰기만 한다.
		Dest[Index] = Vertex;
	}

	RenderCommand::Unmap(Resources.InstanceBuffer.get());
	return true;
}

bool FParticleSystemSceneProxy::GatherDynamicRenderPackets(FRenderQueue& RenderQueue, FRenderQueue*,
	const FVector& CameraLocation, FBillboardMatrixBuilder, const void*,
	TArray<int32>&, TArray<float>&) const
{
	if (!DynamicData || !QuadVertexBuffer || !QuadIndexBuffer)
	{
		return true;
	}

	// 반투명 정렬은 프리미티브 단위 (UE와 같음). 이 시스템의 모든 이미터가 같은 거리를 쓴다.
	const float DistanceSquared = (GetBounds().Center - CameraLocation).SizeSquared();

	const TArray<FDynamicEmitterDataBase*>& EmitterDataArray = DynamicData->DynamicEmitterDataArray;
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterDataArray.Num(); ++EmitterIndex)
	{
		const FDynamicEmitterDataBase* EmitterData = EmitterDataArray[EmitterIndex];
		if (!EmitterData || !EmitterData->bValid || EmitterData->GetSource().eEmitterType != DET_Sprite)
		{
			continue;
		}

		const FDynamicSpriteEmitterReplayDataBase& Source = *static_cast<const FDynamicSpriteEmitterDataBase*>(EmitterData)->GetSourceData();
		UMaterial* Material = Source.MaterialInterface ? Source.MaterialInterface : DefaultMaterial;
		// 인스턴스 입력(INST_)을 읽는 셰이더만 이 경로로 그릴 수 있다.
		if (!Material || Material->ParamLayout != EMaterialParamLayout::ParticleSprite)
		{
			continue;
		}

		int32 ParticleCount = Source.ActiveParticleCount;
		if (Source.MaxDrawCount >= 0)
		{
			ParticleCount = std::min(ParticleCount, Source.MaxDrawCount);
		}
		if (ParticleCount <= 0)
		{
			continue;
		}

		const bool bSorted = Source.SortMode != PSORTMODE_None;
		if (bSorted)
		{
			SortSpriteParticles(Source, Source.ActiveParticleCount, CameraLocation);
		}

		FEmitterRenderResources& Resources = GetEmitterResources(NextResourceIndex++);
		if (!FillInstanceBuffer(Resources, Source, ParticleCount, bSorted))
		{
			continue;
		}

		const float SubImagesH = static_cast<float>(std::max(Source.SubImages_Horizontal, 1));
		const float SubImagesV = static_cast<float>(std::max(Source.SubImages_Vertical, 1));
		Resources.Params.SubImageSize = FVector4(SubImagesH, SubImagesV, 1.0f / SubImagesH, 1.0f / SubImagesV);
		Resources.Params.PivotOffset = Source.PivotOffset;
		Resources.Params.ScreenAlignment = Source.ScreenAlignment;

		// 사각형 1개(인덱스 6개)를 파티클 수만큼 그리는 패킷 하나. Proxy를 비워 Model 행렬을 쓰게 한다.
		FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
		Packet.Model = RenderQueue.StoreWorldMatrix(Source.bUseLocalSpace ? GetLocalToWorld() : FMatrix::Identity);
		Packet.VertexFactory = &Resources.VertexFactory;
		Packet.IndexBuffer = QuadIndexBuffer.get();
		Packet.IndexCount = 6;
		Packet.InstanceCount = static_cast<uint32>(ParticleCount);
		Packet.Material = Material;
		Packet.MaterialParamData = &Resources.Params;
		Packet.MaterialParamDataSize = sizeof(FParticleSpriteUniformParameters);
		Packet.CameraToParticleDistance = DistanceSquared;
		Packet.TranslucencySortPriority = TranslucencySortPriority;
	}
	return true;
}
