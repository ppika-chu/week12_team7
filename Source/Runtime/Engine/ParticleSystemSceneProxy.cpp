#include "EnginePCH.h"
#include "ParticleSystemSceneProxy.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleHelper.h"
#include "Render/Buffer.h"
#include "Render/Material.h"
#include "Render/RenderCommand.h"
#include "Render/RenderPacket.h"

#include <algorithm>
#include <cmath>

namespace
{
// TODO(임시): Size·Color 모듈이 생기기 전까지 쓰는 기본값.
// PreSpawn이 슬롯을 0으로 채우므로, 값을 정해 주는 모듈이 없으면 크기 0·투명이라 화면에 안 보인다.
constexpr float DefaultSpriteSize = 1.0f;

// 사각형 하나의 꼭짓점. ParticleQuad 메시와 같은 배치(X는 정면, Y·Z가 폭·높이)라서 감는 방향도 같다.
struct FQuadCorner
{
	float Y;
	float Z;
	float U;
	float V;
};
constexpr FQuadCorner QuadCorners[4] = {
	{ -0.5f, 0.5f, 0.0f, 0.0f },
	{ 0.5f, 0.5f, 1.0f, 0.0f },
	{ 0.5f, -0.5f, 1.0f, 1.0f },
	{ -0.5f, -0.5f, 0.0f, 1.0f },
};
constexpr uint32 QuadIndices[6] = { 0, 1, 2, 0, 2, 3 };
} // namespace

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* Component)
	: FPrimitiveSceneProxy(Component)
{
	// 정점이 이미 월드 좌표인 일반 정점 버퍼다.
	VertexFactory.Type = EVertexFactoryType::Local;

	Constants.CurrentFrame = 0.0f;
	Constants.AtlasColSize = 1.0f;
	Constants.AtlasRowSize = 1.0f;
	Constants.Alpha = 1.0f;
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy() = default;

void FParticleSystemSceneProxy::AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const
{
	FPrimitiveSceneProxy::AddReferencedMaterials(OutMaterials);
	for (UMaterial* Material : Materials)
	{
		if (Material)
		{
			OutMaterials.Add(Material);
		}
	}
}

// 프레임당 한 번, 첫 뷰가 수집되기 직전에 엔진(PrepareDynamicData)이 부른다.
void FParticleSystemSceneProxy::UpdateDynamicData()
{
	UParticleSystemComponent* ParticleSystemComponent = static_cast<UParticleSystemComponent*>(GetComponent());
	assert(ParticleSystemComponent);

	// 4단계: 시뮬레이션 데이터를 EmitterRenderData로 복사한다.
	ParticleSystemComponent->UpdateDynamicData();

	// 지난 프레임에 모은 정점을 버린다. 이번 프레임의 뷰들이 다시 채운다.
	Vertices.Reset();
	// 지난 프레임의 패킷은 모두 그려졌으므로 교체된 옛 인덱스 버퍼를 지운다.
	RetiredIndexBuffers.Reset();

	Materials.Reset();
	for (const FDynamicEmitterDataBase* DynamicData : ParticleSystemComponent->GetEmitterRenderData())
	{
		if (!DynamicData || DynamicData->GetSource().eEmitterType != DET_Sprite)
		{
			continue;
		}
		const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(DynamicData->GetSource());
		if (UMaterial* Material = static_cast<UMaterial*>(Source.MaterialInterface))
		{
			Materials.Add(Material);
		}
	}
}

// 뷰마다 엔진(FSceneGatherer::GatherDynamicRenderPackets)이 부른다.
// 원본의 GetDynamicMeshElements + GetDynamicMeshElementsEmitter에 해당한다.
bool FParticleSystemSceneProxy::GatherDynamicRenderPackets(FRenderQueue& RenderQueue, FRenderQueue* IconQueue,
	const FVector& CameraLocation, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext,
	TArray<int32>& SortedParticleIndices, TArray<float>& ParticleDistancesSquared) const
{
	if (!BuildBillboardMatrix)
	{
		return false;
	}

	const UParticleSystemComponent* ParticleSystemComponent = static_cast<const UParticleSystemComponent*>(GetComponent());
	assert(ParticleSystemComponent);

	// 다른 반투명 물체와의 앞뒤 순서는 이미터 위치로 정한다. 이미터 안의 순서는 아래에서 직접 정렬한다.
	const FMatrix& LocalToWorld = GetLocalToWorld();
	const FVector EmitterOrigin(LocalToWorld.M[3][0], LocalToWorld.M[3][1], LocalToWorld.M[3][2]);
	const float EmitterDistanceSquared = (EmitterOrigin - CameraLocation).SizeSquared();

	bool bAddedAny = false;

	for (const FDynamicEmitterDataBase* DynamicData : ParticleSystemComponent->GetEmitterRenderData())
	{
		if (!DynamicData || !DynamicData->bValid)
		{
			continue;
		}

		const FDynamicEmitterReplayDataBase& BaseSource = DynamicData->GetSource();
		if (BaseSource.eEmitterType != DET_Sprite)
		{
			continue; // 메시 이미터는 Phase 2
		}
		const FDynamicSpriteEmitterReplayDataBase& Source = static_cast<const FDynamicSpriteEmitterReplayDataBase&>(BaseSource);

		UMaterial* Material = static_cast<UMaterial*>(Source.MaterialInterface);
		if (!Material)
		{
			continue;
		}

		// 그릴 개수 제한 (원본 GetVertexAndIndexData 앞부분)
		int32 ParticleCount = Source.ActiveParticleCount;
		if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
		{
			ParticleCount = Source.MaxDrawCount;
		}
		if (ParticleCount <= 0)
		{
			continue;
		}

		// E2. 정렬. 원본은 SortMode가 None이 아니고, 반투명이거나 나이순 정렬일 때만 한다.
		const bool bTranslucent = Material->BlendState != EBlendState::Opaque;
		int32 SortMode = Source.SortMode;
		if (SortMode == PSORTMODE_None && bTranslucent)
		{
			// TODO: Required 모듈에 SortMode가 생기면 이 대체를 지운다.
			// 그 전까지 반투명은 거리순으로 정렬해 겹침이 깨지지 않게 한다.
			SortMode = PSORTMODE_DistanceToView;
		}

		const FParticleOrder* Order = nullptr;
		if (SortMode != PSORTMODE_None &&
			(bTranslucent || SortMode == PSORTMODE_Age_OldestFirst || SortMode == PSORTMODE_Age_NewestFirst))
		{
			SortSpriteParticles(SortMode, Source.bUseLocalSpace, ParticleCount,
				Source.DataContainer.ParticleData, Source.ParticleStride, Source.DataContainer.ParticleIndices,
				CameraLocation);
			Order = ParticleOrder.GetData();
		}

		// E3. 파티클 → 정점. 이번 뷰의 사각형은 Vertices 뒤에 이어 붙는다.
		const uint32 FirstQuad = static_cast<uint32>(Vertices.Num()) / 4;
		GetVertexAndIndexData(Source, Order, ParticleCount, BuildBillboardMatrix, ViewContext);
		const uint32 QuadCount = static_cast<uint32>(Vertices.Num()) / 4 - FirstQuad;
		if (QuadCount == 0)
		{
			continue;
		}

		// 패킷이 인덱스 버퍼를 포인터로 들고 가므로, 패킷을 만들기 전에 크기를 맞춰 둔다.
		EnsureIndexBuffer(FirstQuad + QuadCount);

		// E4. 이미터당 패킷 하나.
		FRenderPacket Packet;
		Packet.VertexFactory = &VertexFactory;
		Packet.IndexBuffer = IndexBuffer.get();
		Packet.Material = Material;
		Packet.Model = RenderQueue.StoreWorldMatrix(FMatrix::Identity); // 정점이 이미 월드 좌표
		Packet.StartIndex = FirstQuad * 6;
		Packet.IndexCount = QuadCount * 6;
		Packet.CameraToParticleDistance = EmitterDistanceSquared;
		Packet.MaterialParamData = &Constants;
		Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
		RenderQueue.Add(Packet);

		bAddedAny = true;
	}

	if (bAddedAny)
	{
		UploadVertices();
	}

	return true;
}

// 카메라에서 먼 파티클이 앞에 오도록 ParticleOrder를 채운다. (원본 ParticleSystemRender.cpp:196)
// 원본: FDynamicSpriteEmitterDataBase::SortSpriteParticles (ParticleSystemRender.cpp:196)
void FParticleSystemSceneProxy::SortSpriteParticles(int32 SortMode, bool bLocalSpace, int32 ParticleCount,
	const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices,
	const FVector& CameraLocation) const
{
	ParticleOrder.Reset();
	ParticleOrder.Reserve(ParticleCount);

	if (SortMode == PSORTMODE_Age_OldestFirst || SortMode == PSORTMODE_Age_NewestFirst)
	{
		// Flags의 카운터 부분이 생성 순서다. 큰 값(나중에 태어난 것)부터 놓인다.
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE_CONST(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			const uint32 Counter = static_cast<uint32>(Particle.Flags) & static_cast<uint32>(STATE_CounterMask);
			const uint32 Key = (SortMode == PSORTMODE_Age_OldestFirst) ? Counter : (~Counter & static_cast<uint32>(STATE_CounterMask));
			ParticleOrder.Add(FParticleOrder(ParticleIndex, Key));
		}
		std::sort(ParticleOrder.begin(), ParticleOrder.end(),
			[](const FParticleOrder& A, const FParticleOrder& B)
			{
				return A.C > B.C;
			});
		return;
	}

	// PSORTMODE_DistanceToView. PSORTMODE_ViewProjDepth도 뷰 행렬을 받지 않으므로 같은 방식으로 처리한다.
	const FMatrix& LocalToWorld = GetLocalToWorld();
	for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
	{
		DECLARE_PARTICLE_CONST(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
		const FVector Position = bLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
		const float InZ = (CameraLocation - Position).SizeSquared();
		ParticleOrder.Add(FParticleOrder(ParticleIndex, InZ));
	}
	// 먼 것부터 그려야 반투명이 올바르게 겹친다.
	std::sort(ParticleOrder.begin(), ParticleOrder.end(),
		[](const FParticleOrder& A, const FParticleOrder& B)
		{
			return A.Z > B.Z;
		});
}

// 파티클 ParticleCount개를 사각형으로 펼쳐 Vertices 뒤에 붙인다. (원본 ParticleSystemRender.cpp:413)
// Order가 nullptr이면 ParticleIndices 순서 그대로 쓴다.
// 원본: FDynamicSpriteEmitterData::GetVertexAndIndexData (ParticleSystemRender.cpp:413)
void FParticleSystemSceneProxy::GetVertexAndIndexData(const FDynamicSpriteEmitterReplayDataBase& Source, const FParticleOrder* Order,
	int32 ParticleCount, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext) const
{
	const uint8* ParticleData = Source.DataContainer.ParticleData;
	const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;
	const FParticleOrder* OrderedIndices = Order;
	const FMatrix& LocalToWorld = GetLocalToWorld();

	Vertices.Reserve(static_cast<uint32>(Vertices.Num() + ParticleCount * 4));

	for (int32 i = 0; i < ParticleCount; i++)
	{
		const int32 ParticleIndex = OrderedIndices ? OrderedIndices[i].ParticleIndex : i;
		DECLARE_PARTICLE_CONST(Particle, ParticleData + Source.ParticleStride * ParticleIndices[ParticleIndex]);

		// 원본 GetParticleSize: 컴포넌트 스케일을 곱한 절댓값
		FVector Size(std::fabs(Particle.Size.X * Source.Scale.X), std::fabs(Particle.Size.Y * Source.Scale.Y), 0.0f);
		if (Size.X <= 0.0f && Size.Y <= 0.0f)
		{
			Size.X = DefaultSpriteSize; // TODO(임시): Size 모듈이 없을 때
			Size.Y = DefaultSpriteSize;
		}

		FLinearColor Color = Particle.Color;
		if (Color.R == 0.0f && Color.G == 0.0f && Color.B == 0.0f && Color.A == 0.0f)
		{
			Color.R = Color.G = Color.B = Color.A = 1.0f; // TODO(임시): Color 모듈이 없을 때
		}

		FParticleSpriteVertex FillVertex;
		FillVertex.Position = Source.bUseLocalSpace ? LocalToWorld.TransformPosition(Particle.Location) : Particle.Location;
		FillVertex.RelativeTime = Particle.RelativeTime;
		FillVertex.Size = Size;
		FillVertex.Rotation = Particle.Rotation;
		FillVertex.SubImageIndex = 0.0f; // SubUV 모듈이 생기면 페이로드에서 읽는다
		FillVertex.Color = Color;

		AddQuad(FillVertex, BuildBillboardMatrix, ViewContext);
	}
}

// 파티클 하나(FParticleSpriteVertex)를 카메라를 향하는 정점 4개로 펼친다. 원본에서는 셰이더가 하는 일이다.
void FParticleSystemSceneProxy::AddQuad(const FParticleSpriteVertex& Sprite, FBillboardMatrixBuilder BuildBillboardMatrix, const void* ViewContext) const
{
	// 카메라를 향하는 행렬. 이 뷰의 카메라 기준이므로 뷰마다 결과가 다르다.
	const FMatrix Billboard = BuildBillboardMatrix(Sprite.Position, Sprite.Size.X, Sprite.Size.Y, ViewContext);

	// 사각형 평면(Y-Z) 안에서 돌린다.
	const float CosR = std::cos(Sprite.Rotation);
	const float SinR = std::sin(Sprite.Rotation);

	const FVector4 Color(Sprite.Color.R, Sprite.Color.G, Sprite.Color.B, Sprite.Color.A);

	for (const FQuadCorner& Corner : QuadCorners)
	{
		const FVector Local(0.0f, Corner.Y * CosR - Corner.Z * SinR, Corner.Y * SinR + Corner.Z * CosR);

		FParticleSpriteQuadVertex Vertex;
		Vertex.Position = Billboard.TransformPosition(Local);
		Vertex.UV = FVector2(Corner.U, Corner.V);
		Vertex.Color = Color;
		Vertices.Add(Vertex);
	}
}

// Vertices 전체를 GPU 버퍼에 올린다. 모자라면 버퍼를 다시 만든다.
void FParticleSystemSceneProxy::UploadVertices() const
{
	const uint32 VertexCount = static_cast<uint32>(Vertices.Num());
	if (VertexCount == 0)
	{
		return;
	}

	if (VertexCount > VertexCapacity || !VertexBuffer)
	{
		// 매 프레임 다시 만들지 않도록 넉넉히 키운다.
		VertexCapacity = std::max({ VertexCount, VertexCapacity * 2, 256u });
		VertexBuffer = RenderCommand::CreateDynamicVertexBuffer(VertexCapacity * sizeof(FParticleSpriteQuadVertex), sizeof(FParticleSpriteQuadVertex));
		// 패킷은 VertexFactory의 주소를 들고 있으므로, 앞선 뷰의 패킷도 새 버퍼를 보게 된다.
		VertexFactory.VertexBuffer = VertexBuffer.get();
	}

	// 버퍼 전체를 새로 쓴다. 앞선 뷰의 정점도 Vertices에 남아 있으므로 함께 올라간다.
	RenderCommand::UpdateBufferData(VertexBuffer.get(), Vertices.GetData(), VertexCount * sizeof(FParticleSpriteQuadVertex));
}

// 사각형 QuadCount개를 그릴 수 있게 인덱스 버퍼(0,1,2, 0,2,3 반복)를 준비한다.
void FParticleSystemSceneProxy::EnsureIndexBuffer(uint32 QuadCount) const
{
	if (IndexBuffer && QuadCount <= IndexQuadCapacity)
	{
		return;
	}

	// 앞선 뷰의 패킷이 옛 버퍼를 포인터로 들고 있을 수 있다. 그 패킷이 그려질 때까지 살려 둔다.
	// 크기가 커질 때만 일어나므로 몇 번 지나면 더 쌓이지 않는다.
	if (IndexBuffer)
	{
		RetiredIndexBuffers.Add(std::move(IndexBuffer));
	}

	IndexQuadCapacity = std::max({ QuadCount, IndexQuadCapacity * 2, 64u });

	TArray<uint32> Indices;
	Indices.Reserve(IndexQuadCapacity * 6);
	for (uint32 Quad = 0; Quad < IndexQuadCapacity; ++Quad)
	{
		for (const uint32 Index : QuadIndices)
		{
			Indices.Add(Quad * 4 + Index);
		}
	}
	IndexBuffer = RenderCommand::CreateStaticIndexBuffer(Indices.GetData(), static_cast<uint32>(Indices.Num()));
}
