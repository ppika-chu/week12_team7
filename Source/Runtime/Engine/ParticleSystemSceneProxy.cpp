#include "EnginePCH.h"
#include "ParticleSystemSceneProxy.h"
#include "Asset/AssetManager.h"
#include "Core/HAL/UnrealMemory.h"
#include "Render/Mesh.h"
#include "Render/RenderCommand.h"
#include <algorithm>
#include <utility>

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UPrimitiveComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
	, QuadMesh(UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad"))
	, DefaultMaterial(UAssetManager::GetAssetByPath<UMaterial>("ParticleSpriteMaterial"))
{
}

void FParticleSystemSceneProxy::SetDynamicData(TArray<FDynamicSpriteEmitterReplayData>&& InReplayData)
{
	ReplayData = std::move(InReplayData);
	MarkRenderDynamicDataDirty();
}

void FParticleSystemSceneProxy::UpdateDynamicData()
{
	if (!bRenderDynamicDataDirty || !QuadMesh)
	{
		return;
	}

	RenderEmitters.SetNum(ReplayData.Num(), false);
	bool bUploadComplete = true;
	for (int32 Index = 0; Index < ReplayData.Num(); ++Index)
	{
		const FDynamicSpriteEmitterReplayData& Source = ReplayData[Index];
		FEmitterRenderData& Emitter = RenderEmitters[Index];
		const uint32 Count = static_cast<uint32>(Source.Particles.Num());
		Emitter.InstanceCount = 0;
		Emitter.Material = Source.Material ? Source.Material : DefaultMaterial;
		if (Count == 0 || !Emitter.Material)
		{
			continue;
		}

		if (!Emitter.InstanceBuffer || Emitter.InstanceBuffer->GetMaxCount() < Count)
		{
			const uint32 PreviousCapacity = Emitter.InstanceBuffer ? Emitter.InstanceBuffer->GetMaxCount() : 0;
			const uint32 NewCapacity = std::max(Count, PreviousCapacity > 0 ? PreviousCapacity * 2 : 64u);
			auto NewBuffer = RenderCommand::CreateDynamicStructuredBuffer(sizeof(FParticleSpriteInstanceData), NewCapacity);
			if (!NewBuffer || !NewBuffer->GetBuffer() || !NewBuffer->GetSRV())
			{
				bUploadComplete = false;
				continue;
			}
			Emitter.InstanceBuffer = std::move(NewBuffer);
		}

		Emitter.VertexFactory.VertexBuffer = QuadMesh->GetVertexBuffer(0);
		Emitter.VertexFactory.InstanceData = Emitter.InstanceBuffer.get();
		void* Destination = RenderCommand::MapWriteDiscard(Emitter.InstanceBuffer.get());
		if (!Destination)
		{
			bUploadComplete = false;
			continue;
		}

		FMemory::Memcpy(Destination, Source.Particles.GetData(), static_cast<uint64>(Count) * sizeof(FParticleSpriteInstanceData));
		RenderCommand::Unmap(Emitter.InstanceBuffer.get());
		Emitter.InstanceCount = Count;
	}
	bRenderDynamicDataDirty = !bUploadComplete;
}

bool FParticleSystemSceneProxy::GatherDynamicRenderPackets(FRenderQueue& RenderQueue, FRenderQueue*,
	const FVector& CameraLocation, FBillboardMatrixBuilder, const void*, TArray<int32>&, TArray<float>&) const
{
	if (!QuadMesh)
	{
		return true;
	}

	const FVector Delta = GetLocalToWorld().TransformPosition(FVector::ZeroVector) - CameraLocation;
	const float DistanceSquared = Delta.Dot(Delta);
	for (const FEmitterRenderData& Emitter : RenderEmitters)
	{
		if (Emitter.InstanceCount == 0)
		{
			continue;
		}

		FRenderPacket Packet;
		Packet.Proxy = this;
		Packet.VertexFactory = &Emitter.VertexFactory;
		Packet.IndexBuffer = QuadMesh->GetIndexBuffer(0);
		Packet.IndexCount = Packet.IndexBuffer->GetIndexCount();
		Packet.Material = Emitter.Material;
		Packet.InstanceCount = Emitter.InstanceCount;
		Packet.CameraToParticleDistance = DistanceSquared;
		RenderQueue.Add(Packet);
	}
	return true;
}

void FParticleSystemSceneProxy::AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const
{
	FPrimitiveSceneProxy::AddReferencedMaterials(OutMaterials);
	OutMaterials.Add(DefaultMaterial);
	for (const FDynamicSpriteEmitterReplayData& Source : ReplayData)
	{
		OutMaterials.Add(Source.Material);
	}
	for (const FEmitterRenderData& Emitter : RenderEmitters)
	{
		OutMaterials.Add(Emitter.Material);
	}
}