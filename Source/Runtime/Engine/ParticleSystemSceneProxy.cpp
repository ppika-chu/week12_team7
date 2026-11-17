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
#include "Render/RenderResourceManager.h"

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
constexpr uint32 VerticesPerBeam = 4;
constexpr uint32 IndicesPerBeam = ARRAYSIZE(GQuadIndices);

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

UMaterial* GetMeshParticleMaterial(const UStaticMesh& Mesh, const FStaticMeshSection& Section)
{
	UMaterial* Material = Mesh.GetMaterial(Section.MaterialSlotIndex);
	if (Material && Material->ParamLayout == EMaterialParamLayout::StaticMesh
		&& FRenderResourceManager::IsUberLitProgram(Material->Shader))
	{
		return Material;
	}
	return UAssetManager::GetAssetByPath<UMaterial>("DefaultMaterial");
}
}

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, DefaultMaterial(UAssetManager::GetAssetByPath<UMaterial>("ParticleSpriteMaterial"))
	, DefaultBeamMaterial(UAssetManager::GetAssetByPath<UMaterial>("ParticleBeamMaterial"))
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
		if (!EmitterData)
		{
			continue;
		}
		const FDynamicEmitterReplayDataBase& Source = EmitterData->GetSource();
		if (Source.eEmitterType == DET_Sprite)
		{
			if (UMaterial* Material = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(Source).MaterialInterface)
			{
				OutMaterials.Add(Material);
			}
		}
		else if (Source.eEmitterType == DET_Mesh)
		{
			if (const UStaticMesh* Mesh = static_cast<const FDynamicMeshEmitterReplayDataBase&>(Source).Mesh)
			{
				for (const FStaticMeshSection& Section : Mesh->GetMeshData().Sections)
				{
					if (UMaterial* Material = GetMeshParticleMaterial(*Mesh, Section))
					{
						OutMaterials.Add(Material);
					}
				}
			}
		}
		else if (Source.eEmitterType == DET_Beam2)
		{
			const auto& BeamData = static_cast<const FDynamicBeamEmitterReplayDataBase&>(Source);
			if (UMaterial* Material = GetBeamMaterial(BeamData.MaterialInterface))
			{
				OutMaterials.Add(Material);
			}
		}
	}
}

UMaterial* FParticleSystemSceneProxy::GetBeamMaterial(UMaterial* Material) const
{
	if (Material && DefaultBeamMaterial && Material->Shader == DefaultBeamMaterial->Shader
		&& !Material->Textures.IsEmpty() && Material->Textures[0])
	{
		return Material;
	}
	return DefaultBeamMaterial;
}

FParticleSystemSceneProxy::FEmitterRenderResources& FParticleSystemSceneProxy::GetEmitterResources(int32 EmitterIndex) const
{
	while (EmitterResources.Num() <= EmitterIndex)
	{
		EmitterResources.Add(MakeUnique<FEmitterRenderResources>());
	}
	return *EmitterResources[EmitterIndex];
}

// 결과는 ParticleOrder에 남는다.
void FParticleSystemSceneProxy::SortParticles(const FDynamicEmitterReplayDataBase& Source, bool bUseLocalSpace,
	const FVector& CameraLocation, const FVector& WorldSortOffset) const
{
	const FParticleDataContainer& Container = Source.DataContainer;
	const FMatrix& LocalToWorld = GetLocalToWorld();
	const int32 ParticleCount = Source.ActiveParticleCount;
	ParticleOrder.SetNum(ParticleCount);

	if (Source.SortMode == PSORTMODE_ViewProjDepth || Source.SortMode == PSORTMODE_DistanceToView)
	{
		// ViewProjDepth는 UE에서 클립 공간 W(카메라 앞 방향 깊이)지만, 수집 함수가 카메라 위치만 받으므로 거리로 대신한다.
		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[Index]);
			const FVector Position = bUseLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
			ParticleOrder[Index].ParticleIndex = Index;
			ParticleOrder[Index].Z = (CameraLocation - (Position + WorldSortOffset)).SizeSquared();
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

bool FParticleSystemSceneProxy::EnsureDynamicVertexBuffer(FEmitterRenderResources& Resources, uint32 VertexCount, uint32 Stride)
{
	const bool bMatchingStride = Resources.DynamicVertexBuffer && Resources.DynamicVertexBuffer->GetStride() == Stride;
	if (bMatchingStride && Resources.DynamicVertexBuffer->GetBuffer() && Resources.MaxVertices >= VertexCount)
	{
		return true;
	}

	const uint32 PreviousCapacity = bMatchingStride ? Resources.MaxVertices : 0;
	const uint32 NewMax = std::max(VertexCount, std::max<uint32>(PreviousCapacity * 2, 64));
	auto NewBuffer = RenderCommand::CreateDynamicVertexBuffer(NewMax * Stride, Stride);
	if (!NewBuffer || !NewBuffer->GetBuffer())
	{
		return false;
	}
	Resources.DynamicVertexBuffer = std::move(NewBuffer);
	Resources.MaxVertices = NewMax;
	return true;
}

bool FParticleSystemSceneProxy::EnsureBeamIndexBuffer(FEmitterRenderResources& Resources, uint32 BeamCount)
{
	const uint32 Capacity = Resources.BeamIndexBuffer ? Resources.BeamIndexBuffer->GetIndexCount() / IndicesPerBeam : 0;
	if (Capacity >= BeamCount)
	{
		return true;
	}

	const uint32 NewCapacity = std::max(BeamCount, std::max<uint32>(Capacity * 2, 16));
	TArray<uint32> Indices;
	Indices.SetNum(NewCapacity * IndicesPerBeam);
	for (uint32 Beam = 0; Beam < NewCapacity; ++Beam)
	{
		for (uint32 Index = 0; Index < IndicesPerBeam; ++Index)
		{
			Indices[Beam * IndicesPerBeam + Index] = Beam * VerticesPerBeam + GQuadIndices[Index];
		}
	}

	auto Buffer = RenderCommand::CreateStaticIndexBuffer(Indices.GetData(), NewCapacity * IndicesPerBeam);
	if (!Buffer || !Buffer->GetBuffer())
	{
		return false;
	}
	Resources.BeamIndexBuffer = std::move(Buffer);
	return true;
}

bool FParticleSystemSceneProxy::FillInstanceBuffer(FEmitterRenderResources& Resources, const FDynamicSpriteEmitterReplayDataBase& Source, int32 ParticleCount, bool bSorted) const
{
	if (!EnsureDynamicVertexBuffer(Resources, static_cast<uint32>(ParticleCount), sizeof(FParticleSpriteVertex)))
	{
		return false;
	}
	FParticleSpriteVertex* Dest = static_cast<FParticleSpriteVertex*>(RenderCommand::MapWriteDiscard(Resources.DynamicVertexBuffer.get()));
	if (!Dest)
	{
		return false;
	}

	const FParticleDataContainer& Container = Source.DataContainer;
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		// 인스턴스 순서가 곧 그리는 순서다.
		const int32 ParticleIndex = bSorted ? ParticleOrder[Index].ParticleIndex : Index;
		DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[ParticleIndex]);

		FParticleSpriteVertex Vertex;
		Vertex.Position = Particle.Location;
		Vertex.RelativeTime = Particle.RelativeTime;
		Vertex.OldPosition = Particle.OldLocation;
		Vertex.ParticleId = static_cast<float>(static_cast<uint32>(Particle.Flags) & STATE_CounterMask) / 10000.0f;
		Vertex.Size = GetParticleSize(Particle, Source);
		Vertex.Rotation = Particle.Rotation;
		Vertex.SubImageIndex = 0.0f; // SubUV 모듈이 생기면 SubUVDataOffset의 payload에서 읽는다
		Vertex.Color = Particle.Color;

		// 매핑된 메모리는 앞에서부터 쓰기만 한다.
		Dest[Index] = Vertex;
	}

	RenderCommand::Unmap(Resources.DynamicVertexBuffer.get());
	return true;
}

void FParticleSystemSceneProxy::GatherMeshRenderPackets(FRenderQueue& RenderQueue, const FDynamicMeshEmitterReplayDataBase& Source,
	const FVector& CameraLocation, float DistanceSquared) const
{
	const UStaticMesh* Mesh = Source.Mesh;
	const int32 ParticleCount = Source.MaxDrawCount >= 0
		? std::min(Source.ActiveParticleCount, Source.MaxDrawCount)
		: Source.ActiveParticleCount;
	if (!Mesh || ParticleCount <= 0 || Mesh->GetMeshData().Sections.IsEmpty())
	{
		return;
	}
	FVertexBuffer* MeshVertices = Mesh->GetVertexBuffer(0);
	FIndexBuffer* MeshIndices = Mesh->GetIndexBuffer(0);
	if (!MeshVertices || !MeshVertices->GetBuffer() || MeshVertices->GetStride() != sizeof(FVertex)
		|| !MeshIndices || !MeshIndices->GetBuffer())
	{
		return;
	}

	const bool bSorted = Source.SortMode != PSORTMODE_None;
	if (bSorted)
	{
		SortParticles(Source, Source.bUseLocalSpace, CameraLocation);
	}

	FEmitterRenderResources& Resources = GetEmitterResources(NextResourceIndex++);
	if (!EnsureDynamicVertexBuffer(Resources, static_cast<uint32>(ParticleCount), sizeof(FParticleMeshVertex)))
	{
		return;
	}
	auto* Dest = static_cast<FParticleMeshVertex*>(RenderCommand::MapWriteDiscard(Resources.DynamicVertexBuffer.get()));
	if (!Dest)
	{
		return;
	}

	// 로컬 공간에서는 패킷의 World 행렬에 이미 컴포넌트 스케일이 포함된다.
	const FVector WorldSpaceScale = Source.bUseLocalSpace ? FVector(1.0f) : GetComponent()->GetWorldScale3D().GetAbs();
	const FParticleDataContainer& Container = Source.DataContainer;
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		const int32 ParticleIndex = bSorted ? ParticleOrder[Index].ParticleIndex : Index;
		DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[ParticleIndex]);
		Dest[Index] = { Particle.Location, Particle.Size.GetAbs() * WorldSpaceScale,
			FVector(0.0f, 0.0f, Particle.Rotation), Particle.Color };
	}
	RenderCommand::Unmap(Resources.DynamicVertexBuffer.get());

	Resources.VertexFactory.Type = EVertexFactoryType::MeshParticle;
	Resources.VertexFactory.VertexBuffer = MeshVertices;
	Resources.VertexFactory.InstanceBuffer = Resources.DynamicVertexBuffer.get();
	const FMatrix* World = RenderQueue.StoreWorldMatrix(Source.bUseLocalSpace ? GetLocalToWorld() : FMatrix::Identity);
	for (const FStaticMeshSection& Section : Mesh->GetMeshData().Sections)
	{
		UMaterial* Material = GetMeshParticleMaterial(*Mesh, Section);
		if (!Material || Section.IndexCount == 0)
		{
			continue;
		}

		FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
		Packet.Model = World;
		Packet.VertexFactory = &Resources.VertexFactory;
		Packet.IndexBuffer = MeshIndices;
		Packet.StartIndex = Section.StartIndex;
		Packet.IndexCount = Section.IndexCount;
		Packet.InstanceCount = static_cast<uint32>(ParticleCount);
		Packet.Material = Material;
		Packet.CameraToParticleDistance = DistanceSquared;
	}
}

void FParticleSystemSceneProxy::GatherBeamRenderPackets(FRenderQueue& RenderQueue, const FDynamicBeamEmitterReplayDataBase& Source,
	const FVector& CameraLocation, float DistanceSquared) const
{
	UMaterial* Material = GetBeamMaterial(Source.MaterialInterface);
	const int32 ParticleCount = Source.MaxDrawCount >= 0
		? std::min(Source.ActiveParticleCount, Source.MaxDrawCount)
		: Source.ActiveParticleCount;
	if (!Material || !Material->Shader || ParticleCount <= 0 || !std::isfinite(Source.BeamWidth) || Source.BeamWidth <= 0.0f)
	{
		return;
	}

	const FMatrix& LocalToWorld = GetLocalToWorld();
	const FVector EndOffset = LocalToWorld.TransformVector(Source.BeamEndOffset);
	const float LengthSquared = EndOffset.SizeSquared();
	if (!std::isfinite(LengthSquared) || LengthSquared <= SMALL_NUMBER)
	{
		return;
	}
	const FVector BeamAxis = EndOffset.GetSafeNormal();
	const FVector HalfOffset = EndOffset * 0.5f;
	const float HalfWidth = Source.BeamWidth * 0.5f;
	const FVector RefAxis = std::abs(BeamAxis.Z) > 0.99f ? ForwardVector : UpVector;
	const FVector FallbackSide = BeamAxis.Cross(RefAxis).GetSafeNormal();

	const bool bSorted = Source.SortMode != PSORTMODE_None;
	if (bSorted)
	{
		SortParticles(Source, Source.bUseLocalSpace, CameraLocation, HalfOffset);
	}

	const uint32 BeamCount = static_cast<uint32>(ParticleCount);
	FEmitterRenderResources& Resources = GetEmitterResources(NextResourceIndex++);
	if (!EnsureBeamIndexBuffer(Resources, BeamCount)
		|| !EnsureDynamicVertexBuffer(Resources, BeamCount * VerticesPerBeam, sizeof(FParticleBeamVertex)))
	{
		return;
	}
	auto* Dest = static_cast<FParticleBeamVertex*>(RenderCommand::MapWriteDiscard(Resources.DynamicVertexBuffer.get()));
	if (!Dest)
	{
		return;
	}

	const FParticleDataContainer& Container = Source.DataContainer;
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		const int32 ParticleIndex = bSorted ? ParticleOrder[Index].ParticleIndex : Index;
		DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source.ParticleStride * Container.ParticleIndices[ParticleIndex]);
		const FVector Start = Source.bUseLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
		const FVector End = Start + EndOffset;
		const FVector ToCamera = (CameraLocation - (Start + HalfOffset)).GetSafeNormal();
		const FVector Side = BeamAxis.Cross(ToCamera).GetSafeNormal(SMALL_NUMBER, FallbackSide) * HalfWidth;

		// 같은 인덱스 순서로 카메라를 향하는 사각형을 만든다.
		Dest[0] = { Start + Side, FVector2(0.0f, 0.0f), Particle.Color };
		Dest[1] = { End + Side,   FVector2(1.0f, 0.0f), Particle.Color };
		Dest[2] = { End - Side,   FVector2(1.0f, 1.0f), Particle.Color };
		Dest[3] = { Start - Side, FVector2(0.0f, 1.0f), Particle.Color };
		Dest += VerticesPerBeam;
	}
	RenderCommand::Unmap(Resources.DynamicVertexBuffer.get());

	Resources.VertexFactory.Type = EVertexFactoryType::Local;
	Resources.VertexFactory.VertexBuffer = Resources.DynamicVertexBuffer.get();
	Resources.VertexFactory.InstanceBuffer = nullptr;
	FRenderPacket& Packet = RenderQueue.AddDefaulted_GetRef();
	Packet.VertexFactory = &Resources.VertexFactory;
	Packet.IndexBuffer = Resources.BeamIndexBuffer.get();
	Packet.IndexCount = BeamCount * IndicesPerBeam;
	Packet.Material = Material;
	Packet.CameraToParticleDistance = DistanceSquared;
}

bool FParticleSystemSceneProxy::GatherDynamicRenderPackets(FRenderQueue& RenderQueue, FRenderQueue*,
	const FVector& CameraLocation, FBillboardMatrixBuilder, const void*,
	TArray<int32>&, TArray<float>&) const
{
	if (!DynamicData)
	{
		return true;
	}

	// 반투명 정렬은 프리미티브 단위 (UE와 같음). 이 시스템의 모든 이미터가 같은 거리를 쓴다.
	const float DistanceSquared = (GetBounds().Center - CameraLocation).SizeSquared();

	const TArray<FDynamicEmitterDataBase*>& EmitterDataArray = DynamicData->DynamicEmitterDataArray;
	for (int32 EmitterIndex = 0; EmitterIndex < EmitterDataArray.Num(); ++EmitterIndex)
	{
		const FDynamicEmitterDataBase* EmitterData = EmitterDataArray[EmitterIndex];
		if (!EmitterData || !EmitterData->bValid)
		{
			continue;
		}

		const FDynamicEmitterReplayDataBase& ReplayData = EmitterData->GetSource();
		if (ReplayData.eEmitterType == DET_Mesh)
		{
			GatherMeshRenderPackets(RenderQueue, static_cast<const FDynamicMeshEmitterReplayDataBase&>(ReplayData),
				CameraLocation, DistanceSquared);
			continue;
		}
		if (ReplayData.eEmitterType == DET_Beam2)
		{
			GatherBeamRenderPackets(RenderQueue, static_cast<const FDynamicBeamEmitterReplayDataBase&>(ReplayData),
				CameraLocation, DistanceSquared);
			continue;
		}
		if (ReplayData.eEmitterType != DET_Sprite || !QuadVertexBuffer || !QuadIndexBuffer)
		{
			continue;
		}
		const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(ReplayData);
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
			SortParticles(Source, Source.bUseLocalSpace, CameraLocation);
		}

		FEmitterRenderResources& Resources = GetEmitterResources(NextResourceIndex++);
		if (!FillInstanceBuffer(Resources, Source, ParticleCount, bSorted))
		{
			continue;
		}
		Resources.VertexFactory.Type = EVertexFactoryType::Local;
		Resources.VertexFactory.VertexBuffer = QuadVertexBuffer.get();
		Resources.VertexFactory.InstanceBuffer = Resources.DynamicVertexBuffer.get();

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
	}
	return true;
}
