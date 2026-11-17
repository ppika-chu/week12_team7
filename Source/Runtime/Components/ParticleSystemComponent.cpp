#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"

#include "Math/Noise.h"
#include "Math/Transform.h"
#include "Asset/AssetManager.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include <algorithm>
#include <cmath>
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleTypeDataBeam.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Engine/World.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCastShadow(false);
	SetReceiveShadow(false);
	SetReceivesDecals(false);
	Template = UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem");
	// UE와 같이 꺼진 상태로 시작하고 Activate로 켠다. 켜져 있으면 ShouldActivate가 false라 Activate()가 무시된다.
	SetActive(false);
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles(true);
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::OnRegister()
{
	Super::OnRegister();
	bJustRegistered = true;
	if (Template && (bAutoActivate || IsActive()))
	{
		ActivateSystem(true);
	}
	// BeginPlay 이후 재등록된 컴포넌트도 다시 Tick한다.
	if (GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		PrimaryComponentTick.RegisterTickFunction(GetWorld()->GetTickTaskManager());
	}
}

void UParticleSystemComponent::OnUnregister()
{
	PrimaryComponentTick.UnRegisterTickFunction();
	ResetParticles(true);
	Super::OnUnregister();
}

void UParticleSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoActivate)
	{
		Activate();
	}
}

bool UParticleSystemComponent::ShouldActivate() const
{
	// 꺼져 있거나, 정지·완료된 상태면 다시 켤 수 있다.
	return Super::ShouldActivate() || bWasDeactivated || bWasCompleted;
}

void UParticleSystemComponent::Activate(bool bReset)
{
	if (Template && (bReset || ShouldActivate()))
	{
		ActivateSystem(bReset);
	}
}

void UParticleSystemComponent::Deactivate()
{
	// 켜져 있을 때만 끈다 (UE와 같음). 이미 꺼졌거나 정지·완료된 상태면 무시한다.
	if (!ShouldActivate())
	{
		DeactivateSystem();
	}
}

void UParticleSystemComponent::EndPlay()
{
	ResetParticles(true);
	Super::EndPlay();
}

void UParticleSystemComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	MaxParticles = std::clamp(MaxParticles, 1, FParticleDataContainer::MaxCapacity);
	if (Property.Name == "Template" || Property.Name == "MaxParticles" || Property.Name == "RandomSeed" || Property.Name == "EmitterDelay")
	{
		SetTemplate(Template);
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	const bool bWasActive = IsActive();
	ResetParticles(true);
	Template = NewTemplate;
	SetActive(false);
	if (bWasActive && IsRegistered())
	{
		Activate(true);
	}
}

void UParticleSystemComponent::InitializeSystem()
{
	if (!Template)
	{
		return;
	}
	MaxParticles = std::clamp(MaxParticles, 1, FParticleDataContainer::MaxCapacity);

	// 인스턴스가 Duration을 계산할 때 난수를 쓰므로 먼저 시드를 정한다.
	const int32 Seed = (RandomSeed != 0) ? RandomSeed : static_cast<int32>(FMath::FRand() * 2147483646.0f) + 1;
	RandomStream.Initialize(Seed);

	InitParticles();
}

void UParticleSystemComponent::InitParticles()
{
	// 이미터 순서와 인스턴스 순서를 맞춘다. 만들지 못한 이미터는 nullptr로 자리만 둔다.
	const int32 NumEmitters = Template->Emitters.Num();
	if (EmitterInstances.Num() != NumEmitters)
	{
		ResetParticles(true);
		EmitterInstances.SetNum(NumEmitters);
	}

	for (int32 Index = 0; Index < NumEmitters; ++Index)
	{
		UParticleEmitter* Emitter = Template->Emitters[Index];
		FParticleEmitterInstance*& Instance = EmitterInstances[Index];

		// 다른 이미터용으로 만든 인스턴스는 재사용할 수 없다.
		if (Instance && (!Emitter || Instance->SpriteTemplate != Emitter))
		{
			delete Instance;
			Instance = nullptr;
		}
		if (!Emitter)
		{
			continue;
		}

		if (Instance)
		{
			// UE와 같이 메모리와 살아 있는 파티클은 그대로 두고, 시간·Burst·루프 길이만 처음으로 되돌린다.
			Instance->bEnabled = true;
			Instance->bHaltSpawning = false;
			Instance->InitParameters(Emitter);
			Instance->Init();
		}
		else
		{
			Instance = Emitter->CreateInstance(*this);
		}
	}
}

void UParticleSystemComponent::ActivateSystem(bool bFlagAsJustAttached)
{
	if (!Template)
	{
		return;
	}

	if (bFlagAsJustAttached)
	{
		bJustRegistered = true;
	}

	const UWorld* World = GetWorld();
	const bool bIsGameWorld = World && (World->GetWorldType() == EWorldType::Game || World->GetWorldType() == EWorldType::PIE);

	// UE와 같이 게임에서는 다시 켤 때마다 전부 다시 초기화하고(인스턴스는 재사용), 에디터에서는 시간만 되감는다.
	if (EmitterInstances.IsEmpty() || (bIsGameWorld && (!bAutoActivate || bHasBeenActivated)))
	{
		InitializeSystem();
	}
	else if (!bIsGameWorld)
	{
		for (FParticleEmitterInstance* Instance : EmitterInstances)
		{
			if (Instance)
			{
				Instance->Rewind();
				Instance->bHaltSpawning = false;
			}
		}
	}

	bWasDeactivated = false;
	bWasCompleted = false;
	TimeUntilLODCheck = 0.0f;
	SetActive(true);
	bHasBeenActivated = true;
	MarkRenderDynamicDataDirty();
}

void UParticleSystemComponent::DeactivateSystem()
{
	bWasDeactivated = true;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
		{
			continue;
		}
		// 남은 파티클은 수명이 다할 때까지 두고 생성만 멈춘다. bKillOnDeactivate면 바로 지운다.
		Instance->bHaltSpawning = true;
		if (Instance->bKillOnDeactivate)
		{
			Instance->KillParticlesForced();
		}
	}
	SetActive(false);
	bHasBounds = false;
	MarkRenderDynamicDataDirty();
	Super::OnTransformDirty();
}

void UParticleSystemComponent::ResetParticles(bool bEmptyInstances)
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
		{
			continue;
		}
		if (bEmptyInstances)
		{
			delete Instance;
		}
		else
		{
			Instance->KillParticlesForced();
		}
	}
	if (bEmptyInstances)
	{
		EmitterInstances.Empty();
	}
	bHasBounds = false;
	Super::OnTransformDirty();
	// 프록시가 들고 있는 스냅샷도 비우게 한다.
	MarkRenderDynamicDataDirty();
}

FParticleDynamicData* UParticleSystemComponent::CreateDynamicData()
{
	FParticleDynamicData* NewData = new FParticleDynamicData();
	const FVector Scale = GetWorldScale3D().GetAbs();
	if (Scale.X == 0.0f || Scale.Y == 0.0f || Scale.Z == 0.0f)
	{
		return NewData;
	}
	for (int32 Index = 0; Index < EmitterInstances.Num(); ++Index)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[Index];
		if (!Instance)
		{
			continue;
		}
		// 그릴 파티클이 없는 이미터는 nullptr을 돌려주므로 목록에 넣지 않는다.
		if (FDynamicEmitterDataBase* EmitterData = Instance->GetDynamicData(false))
		{
			EmitterData->EmitterIndex = Index;
			if (EmitterData->GetSource().eEmitterType == DET_Sprite)
			{
				static_cast<FDynamicSpriteEmitterData*>(EmitterData)->Source.Scale = Scale;
			}
			NewData->DynamicEmitterDataArray.Add(EmitterData);
		}
	}
	return NewData;
}

void UParticleSystemComponent::KillParticlesForced()
{
	ResetParticles(false);
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (EmitterInstances.IsEmpty() || bWasCompleted || (!IsActive() && !bWasDeactivated))
	{
		return;
	}

	if (IsActive())
	{
		UpdateLOD(DeltaTime);
	}

	bool bAllEmittersDone = true;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
		{
			continue;
		}

		Instance->Tick(DeltaTime, bWasDeactivated);

		if (Instance->bKillOnCompleted && Instance->HasCompleted())
		{
			Instance->KillParticlesForced();
		}
		if (!Instance->bEmitterIsDone)
		{
			bAllEmittersDone = false;
		}
	}
	bJustRegistered = false;

	// 생성을 멈춘 뒤 남은 파티클까지 모두 사라졌으면 끝난 것으로 본다.
	if (bWasDeactivated && GetNumActiveParticles() == 0)
	{
		bAllEmittersDone = true;
	}

	if (bAllEmittersDone)
	{
		bWasCompleted = true;
		SetActive(false);
		bHasBounds = false;
	}

	UpdateBoundsIfNeeded(DeltaTime);

	// 시뮬레이션이 바뀌었음을 알린다. 프록시가 그릴 때 CreateDynamicData로 스냅샷을 받아 간다.
	MarkRenderDynamicDataDirty();
}

void UParticleSystemComponent::UpdateLOD(float DeltaTime)
{
	if (!Template || Template->LODDistances.Num() < 2 || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || (World->GetWorldType() != EWorldType::Game && World->GetWorldType() != EWorldType::PIE))
	{
		return;
	}

	TimeUntilLODCheck -= DeltaTime;
	if (TimeUntilLODCheck > 0.0f)
	{
		return;
	}

	const APlayerCameraManager* CameraManager = World->GetPlayerCameraManager();
	if (!CameraManager || !CameraManager->HasValidCameraView())
	{
		return;
	}

	const float Distance = (CameraManager->GetCameraView().GetLocation() - GetComponentLocation()).Size();
	if (!std::isfinite(Distance))
	{
		return;
	}
	TimeUntilLODCheck = Template->LODDistanceCheckTime;

	const TArray<float>& Distances = Template->LODDistances;
	const auto NextThreshold = std::upper_bound(Distances.begin(), Distances.end(), Distance);
	const int32 SystemLODIndex = std::max(0, static_cast<int32>(NextThreshold - Distances.begin()) - 1);
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || !Instance->SpriteTemplate)
		{
			continue;
		}

		const int32 MaxLODIndex = Instance->SpriteTemplate->GetLODLevels().Num() - 1;
		const int32 EmitterLODIndex = std::min(SystemLODIndex, MaxLODIndex);
		if (EmitterLODIndex >= 0 && EmitterLODIndex != Instance->CurrentLODLevelIndex)
		{
			Instance->SetCurrentLODIndex(EmitterLODIndex);
		}
	}
}

void UParticleSystemComponent::UpdateBoundsIfNeeded(float DeltaTime)
{
	bool bIsTransformDirty = !bHasBounds;
	if (!bIsTransformDirty && !bWasCompleted)
	{
		// 가끔은 강제로 다시 계산해 커졌던 바운드를 줄인다.
		TimeSinceLastForceUpdateTransform += DeltaTime;
		if (TimeSinceLastForceUpdateTransform > MaxTimeBeforeForceUpdateTransform)
		{
			bIsTransformDirty = true;
		}
		else
		{
			// 커졌을 때만 다시 계산한다. 작아진 것은 위의 강제 갱신에서 반영된다.
			FBox ParticleBounds;
			if (CalcParticleWorldBounds(ParticleBounds)
				&& (!Bounds.IsInside(ParticleBounds.Min) || !Bounds.IsInside(ParticleBounds.Max)))
			{
				bIsTransformDirty = true;
			}
		}
	}

	if (bIsTransformDirty)
	{
		// 비교 기준을 새로 잡고, 프록시가 다음 렌더 전에 CalcLocalBounds로 같은 값을 다시 계산하게 한다.
		Bounds = CalcWorldBounds();
		bHasBounds = true;
		Super::OnTransformDirty();
		TimeSinceLastForceUpdateTransform = 0.0f;
	}
}

bool UParticleSystemComponent::HasCompleted() const
{
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && !Instance->HasCompleted())
		{
			return false;
		}
	}
	return true;
}

int32 UParticleSystemComponent::GetNumActiveParticles() const
{
	int32 Total = 0;
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Total += Instance->ActiveParticles;
		}
	}
	return Total;
}

bool UParticleSystemComponent::CalcParticleWorldBounds(FBox& OutBounds) const
{
	bool bValid = false;
	const FVector Scale = GetWorldScale3D().GetAbs();
	const FMatrix ComponentToWorld = GetWorldMatrix();
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || Instance->ActiveParticles <= 0 || !Instance->CurrentLODLevel || !Instance->CurrentLODLevel->bEnabled)
		{
			continue;
		}
		const UParticleModuleRequired* Required = Instance->CurrentLODLevel->RequiredModule;
		const bool bUseLocalSpace = Required && Required->bUseLocalSpace;
		const auto* RibbonTypeData = Cast<UParticleModuleTypeDataRibbon>(Instance->CurrentLODLevel->TypeDataModule);
		const float RibbonHalfWidth = RibbonTypeData ? RibbonTypeData->Width * 0.5f : 0.0f;
		if (RibbonTypeData && (Instance->ActiveParticles < 2 || !std::isfinite(RibbonHalfWidth) || RibbonHalfWidth <= 0.0f))
		{
			continue;
		}
		const auto* BeamTypeData = Cast<UParticleModuleTypeDataBeam>(Instance->CurrentLODLevel->TypeDataModule);
		const FVector BeamEndOffset = BeamTypeData ? ComponentToWorld.TransformVector(BeamTypeData->BeamEndOffset) : FVector::ZeroVector;
		const float BeamHalfWidth = BeamTypeData ? BeamTypeData->BeamWidth * 0.5f : 0.0f;
		const float BeamLengthSquared = BeamEndOffset.SizeSquared();
		if (BeamTypeData && (!std::isfinite(BeamHalfWidth) || BeamHalfWidth <= 0.0f
			|| !std::isfinite(BeamLengthSquared) || BeamLengthSquared <= SMALL_NUMBER))
		{
			continue;
		}
		const auto* MeshTypeData = Cast<UParticleModuleTypeDataMesh>(Instance->CurrentLODLevel->TypeDataModule);
		if (MeshTypeData && !MeshTypeData->Mesh)
		{
			continue;
		}
		const FBox* MeshBounds = MeshTypeData ? &MeshTypeData->Mesh->GetRenderBounds() : nullptr;
		const FVector MeshSizeScale = bUseLocalSpace ? FVector::OneVector : Scale;
		const bool bSquare = Required && (Required->ScreenAlignment == PSA_Square
			|| Required->ScreenAlignment == PSA_FacingCameraPosition || Required->ScreenAlignment == PSA_FacingCameraDistanceBlend);
		for (int32 Index = 0; Index < Instance->ActiveParticles; ++Index)
		{
			DECLARE_PARTICLE_CONST(Particle, Instance->ParticleData + static_cast<uint64>(Instance->ParticleStride) * Instance->ParticleIndices[Index]);
			FBox ParticleBounds;
			if (MeshBounds)
			{
				// Mesh 렌더링과 같은 순서로 입자 스케일, Z 회전, 위치를 적용한다.
				const FQuat Rotation = FQuat::MakeFromAxisAngle(UpVector, Particle.Rotation);
				FMatrix MeshToWorld = FTransform(Rotation, Particle.Location, Particle.Size.GetAbs() * MeshSizeScale).GetLocalMatrix();
				if (bUseLocalSpace)
				{
					MeshToWorld *= ComponentToWorld;
				}
				ParticleBounds = MeshBounds->GetWorldAABB(MeshToWorld);
			}
			else if (BeamTypeData)
			{
				const FVector Start = bUseLocalSpace ? ComponentToWorld.TransformPosition(Particle.Location) : Particle.Location;
				const FVector End = Start + BeamEndOffset;
				const FVector Padding(BeamHalfWidth);
				// 카메라에 따라 폭 방향이 달라져도 두 끝점의 사각형을 모두 감싼다.
				ParticleBounds = FBox{ FVector::Min(Start, End) - Padding, FVector::Max(Start, End) + Padding };
			}
			else if (RibbonTypeData)
			{
				const FVector Center = bUseLocalSpace ? ComponentToWorld.TransformPosition(Particle.Location) : Particle.Location;
				if (!std::isfinite(Center.X) || !std::isfinite(Center.Y) || !std::isfinite(Center.Z))
				{
					continue;
				}
				const FVector Padding(RibbonHalfWidth);
				ParticleBounds = FBox{ Center - Padding, Center + Padding };
			}
			else
			{
				const FVector Center = bUseLocalSpace ? ComponentToWorld.TransformPosition(Particle.Location) : Particle.Location;
				const float Width = Particle.Size.X * Scale.X;
				const float Height = bSquare ? Width : Particle.Size.Y * Scale.Y;
				// 스프라이트의 회전과 카메라 방향에 관계없이 감싸는 반지름.
				const float Radius = 0.5f * std::sqrt(Width * Width + Height * Height);
				const FVector Extent(Radius, Radius, Radius);
				ParticleBounds = FBox{ Center - Extent, Center + Extent };
			}
			if (!bValid)
			{
				OutBounds = ParticleBounds;
				bValid = true;
			}
			else
			{
				OutBounds.Expand(ParticleBounds);
			}
		}
	}
	if (!bValid)
	{
		return false;
	}

	// 퍼져 나가는 이펙트에서 바운드가 조금 더 오래 유효하도록 10% 넓힌다.
	const FVector ExpandAmount = (OutBounds.Max - OutBounds.Min) * 0.5f * 0.1f;
	OutBounds.Min = OutBounds.Min - ExpandAmount;
	OutBounds.Max = OutBounds.Max + ExpandAmount;
	return true;
}

// UE CalcBounds. 파티클이 없으면 컴포넌트 위치의 크기 0 상자
FBox UParticleSystemComponent::CalcWorldBounds() const
{
	FBox WorldBounds;
	if (!CalcParticleWorldBounds(WorldBounds))
	{
		const FVector Location = GetWorldLocation();
		WorldBounds = FBox{ Location, Location };
	}
	return WorldBounds;
}

// 프록시가 UpdateTransform에서 부른다. 호출하는 쪽이 월드 행렬을 곱하므로 로컬로 되돌려 넘긴다.
FBox UParticleSystemComponent::CalcLocalBounds() const
{
	const FVector Scale = GetWorldScale3D().GetAbs();
	if (Scale.X == 0.0f || Scale.Y == 0.0f || Scale.Z == 0.0f)
	{
		return FBox{ FVector::ZeroVector, FVector::ZeroVector };
	}
	return CalcWorldBounds().GetWorldAABB(GetWorldMatrix().Inverse());
}

void UParticleSystemComponent::OnTransformDirty()
{
	bHasBounds = false;
	MarkRenderDynamicDataDirty();
	Super::OnTransformDirty();
}

bool UParticleSystemComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	const FBox LocalBounds = CalcLocalBounds();
	float HitT;
	if (!RayIntersectsAABB(LocalRay, LocalBounds.Min, LocalBounds.Max, HitT) || HitT >= OutT)
	{
		return false;
	}
	OutT = HitT;
	return true;
}

void UParticleSystemComponent::AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const
{
	Super::AddReferencedMaterials(OutMaterials);
	if (!Template)
	{
		return;
	}
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		if (!Emitter)
		{
			continue;
		}
		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (LOD && LOD->RequiredModule)
			{
				OutMaterials.Add(LOD->RequiredModule->Material);
			}
		}
	}
}

FVector UParticleSystemComponent::GetComponentLocation() const
{
	return GetWorldLocation();
}

FMatrix UParticleSystemComponent::GetComponentToWorld() const
{
	return GetWorldMatrix();
}
