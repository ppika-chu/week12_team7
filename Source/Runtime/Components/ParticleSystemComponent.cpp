#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"

#include "Math/Noise.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Engine/ParticleSystemSceneProxy.h"
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

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
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
	// 프록시가 들고 있는 스냅샷도 비우게 한다.
	MarkRenderDynamicDataDirty();
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

	UpdateBoundsIfNeeded(DeltaTime);

	// 시뮬레이션이 바뀌었음을 알린다. 프록시가 그릴 때 CreateDynamicData로 스냅샷을 받아 간다.
	MarkRenderDynamicDataDirty();
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
		OnTransformDirty();
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
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || Instance->ActiveParticles <= 0)
		{
			continue;
		}
		// 인스턴스의 범위는 로컬 공간 이미터여도 월드 좌표다.
		if (!bValid)
		{
			OutBounds = Instance->ParticleBoundingBox;
			bValid = true;
		}
		else
		{
			OutBounds.Expand(Instance->ParticleBoundingBox);
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
	return CalcWorldBounds().GetWorldAABB(GetWorldMatrix().Inverse());
}

FVector UParticleSystemComponent::GetComponentLocation() const
{
	return GetWorldLocation();
}

FMatrix UParticleSystemComponent::GetComponentToWorld() const
{
	return GetWorldMatrix();
}
