#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"

#include <cfloat>
#include <cmath>

#include "Math/Noise.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Render/LineBatcher.h"
#include "Engine/World.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// UE와 같이 꺼진 상태로 시작하고 Activate로 켠다. 켜져 있으면 ShouldActivate가 false라 Activate()가 무시된다.
	SetActive(false);
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles(true);
}

void UParticleSystemComponent::OnRegister()
{
	Super::OnRegister();
	bJustRegistered = true;
}

void UParticleSystemComponent::OnUnregister()
{
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
	if (Property.Name == "Template")
	{
		SetTemplate(Template);
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	const bool bWasActive = IsActive() && !EmitterInstances.IsEmpty();
	ResetParticles(true);
	Template = NewTemplate;
	if (bWasActive)
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
	SetActive(true);
	bHasBeenActivated = true;
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
	DynamicData.reset();
}

FParticleDynamicData* UParticleSystemComponent::CreateDynamicData()
{
	FParticleDynamicData* NewData = new FParticleDynamicData();
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

	if (EmitterInstances.IsEmpty() || bWasCompleted)
	{
		return;
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
	}

	// 시뮬레이션이 끝난 시점에 스냅샷을 새로 떠서 렌더 쪽에 알린다.
	DynamicData.reset(CreateDynamicData());
	MarkRenderDynamicDataDirty();
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

FBox UParticleSystemComponent::CalcLocalBounds() const
{
	FVector WorldMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool bHasParticles = false;

	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->ActiveParticles > 0)
		{
			WorldMin = FVector::Min(WorldMin, Instance->ParticleBoundingBox.Min);
			WorldMax = FVector::Max(WorldMax, Instance->ParticleBoundingBox.Max);
			bHasParticles = true;
		}
	}

	if (!bHasParticles)
	{
		return FBox{ FVector(-0.5f), FVector(0.5f) };
	}

	// 인스턴스의 범위는 월드 좌표다. 호출하는 쪽이 월드 행렬을 곱하므로 로컬로 되돌려 넘긴다.
	const FMatrix WorldToLocal = GetWorldMatrix().Inverse();
	FVector LocalMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector LocalMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector WorldCorner(
			(Corner & 1) ? WorldMax.X : WorldMin.X,
			(Corner & 2) ? WorldMax.Y : WorldMin.Y,
			(Corner & 4) ? WorldMax.Z : WorldMin.Z);
		const FVector LocalCorner = WorldToLocal.TransformPosition(WorldCorner);
		LocalMin = FVector::Min(LocalMin, LocalCorner);
		LocalMax = FVector::Max(LocalMax, LocalCorner);
	}
	return FBox{ LocalMin, LocalMax };
}

void UParticleSystemComponent::DrawDebug(FLineBatcher& LineBatcher) const
{
	if (!DynamicData)
	{
		return;
	}

	constexpr int32 MaxDebugParticles = 2000;
	const FMatrix ComponentToWorld = GetWorldMatrix();
	int32 Drawn = 0;

	// 인스턴스가 아니라 스냅샷을 읽는다. 렌더가 받을 데이터와 같은 것을 그려서 스냅샷이 맞는지 확인한다.
	for (const FDynamicEmitterDataBase* EmitterData : DynamicData->DynamicEmitterDataArray)
	{
		if (!EmitterData || EmitterData->GetSource().eEmitterType != DET_Sprite)
		{
			continue;
		}

		const FDynamicSpriteEmitterReplayDataBase* Source = static_cast<const FDynamicSpriteEmitterDataBase*>(EmitterData)->GetSourceData();
		const FParticleDataContainer& Container = Source->DataContainer;
		const bool bUseLocalSpace = Source->bUseLocalSpace;
		for (int32 Index = 0; Index < Source->ActiveParticleCount && Drawn < MaxDebugParticles; ++Index, ++Drawn)
		{
			DECLARE_PARTICLE_CONST(Particle, Container.ParticleData + Source->ParticleStride * Container.ParticleIndices[Index]);

			const FVector Location = bUseLocalSpace ? ComponentToWorld.TransformPosition(Particle.Location) : Particle.Location;
			const float Half = FMath::Max(std::abs(Particle.Size.X) * 0.5f, 0.02f);
			const FVector4 Color(Particle.Color.R, Particle.Color.G, Particle.Color.B, 1.0f);

			LineBatcher.AddLine(Location - FVector(Half, 0.0f, 0.0f), Location + FVector(Half, 0.0f, 0.0f), Color);
			LineBatcher.AddLine(Location - FVector(0.0f, Half, 0.0f), Location + FVector(0.0f, Half, 0.0f), Color);
			LineBatcher.AddLine(Location - FVector(0.0f, 0.0f, Half), Location + FVector(0.0f, 0.0f, Half), Color);
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
