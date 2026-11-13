#include "EnginePCH.h"
#include "Particles/ParticleEmitterInstances.h"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "Math/RandomStream.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"

namespace
{
constexpr int32 MaxParticlesPerEmitter = 65535; // ParticleIndices가 uint16이라서
constexpr float TwoPi = 6.28318530718f;

int32 AlignTo16(int32 Size)
{
	return (Size + 15) & ~15;
}
}

FParticleEmitterInstance::FParticleEmitterInstance(IParticleEmitterInstanceOwner& InComponent)
	: Component(InComponent)
{
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	std::free(ParticleData);
	std::free(ParticleIndices);
	std::free(InstanceData);
}

void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate)
{
	SpriteTemplate = InTemplate;
	SetupEmitterDuration();
}

void FParticleEmitterInstance::Init()
{
	UParticleLODLevel* HighLODLevel = SpriteTemplate->GetLODLevel(0);
	assert(HighLODLevel && HighLODLevel->RequiredModule);

	CurrentLODLevelIndex = 0;
	CurrentLODLevel = HighLODLevel;
	CurrentMaterial = HighLODLevel->RequiredModule->Material;

	const bool bNeedsInit = (ParticleSize == 0);
	if (bNeedsInit)
	{
		// 칸 크기를 16의 배수로 올린다.
		ParticleSize = AlignTo16(SpriteTemplate->ParticleSize);
		ParticleStride = ParticleSize;
		TypeDataOffset = SpriteTemplate->TypeDataOffset;
		TypeDataInstanceOffset = SpriteTemplate->TypeDataInstanceOffset;

		if (!InstanceData || SpriteTemplate->ReqInstanceBytes > InstancePayloadSize)
		{
			InstanceData = static_cast<uint8*>(std::realloc(InstanceData, SpriteTemplate->ReqInstanceBytes));
			InstancePayloadSize = SpriteTemplate->ReqInstanceBytes;
		}
		if (InstanceData)
		{
			std::memset(InstanceData, 0, InstancePayloadSize);
		}
		for (UParticleModule* Module : SpriteTemplate->ModulesNeedingInstanceData)
		{
			if (uint8* PrepInstData = GetModuleInstanceData(Module))
			{
				Module->PrepPerInstanceBlock(this, PrepInstData);
			}
		}

		bKillOnDeactivate = HighLODLevel->RequiredModule->bKillOnDeactivate;
		bKillOnCompleted = HighLODLevel->RequiredModule->bKillOnCompleted;

		BurstFired.SetNum(SpriteTemplate->LODLevels.Num());
		for (int32 LODIndex = 0; LODIndex < SpriteTemplate->LODLevels.Num(); ++LODIndex)
		{
			UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels[LODIndex];
			const int32 BurstCount = (LODLevel && LODLevel->SpawnModule) ? LODLevel->SpawnModule->BurstList.Num() : 0;
			BurstFired[LODIndex].SetNum(BurstCount);
		}
	}

	SpawnFraction = 0.0f;
	SecondsSinceCreation = 0.0f;
	EmitterTime = 0.0f;
	ParticleCounter = 0;
	LoopCount = 0;

	Location = Component.GetComponentLocation();
	OldLocation = Location;

	if (!ParticleData)
	{
		MaxActiveParticles = 0;
		ActiveParticles = 0;
	}
	ParticleBoundingBox = FBox();

	if (bNeedsInit)
	{
		int32 InitialCount = 10;
		if (SpriteTemplate->InitialAllocationCount > 0)
		{
			InitialCount = FMath::Min(SpriteTemplate->InitialAllocationCount, 100);
		}
		else if (HighLODLevel->PeakActiveParticles > 0)
		{
			InitialCount = FMath::Min(HighLODLevel->PeakActiveParticles, 100);
		}
		Resize(InitialCount);
	}

	ResetBurstList();
	bEmitterIsDone = false;
}

bool FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles)
{
	if (NewMaxActiveParticles < 0 || NewMaxActiveParticles > MaxParticlesPerEmitter)
	{
		return false;
	}

	if (NewMaxActiveParticles > MaxActiveParticles)
	{
		uint8* NewParticleData = static_cast<uint8*>(std::realloc(ParticleData, static_cast<size_t>(ParticleStride) * NewMaxActiveParticles));
		if (!NewParticleData)
		{
			return false;
		}
		ParticleData = NewParticleData;

		if (!ParticleIndices)
		{
			MaxActiveParticles = 0;
		}
		uint16* NewIndices = static_cast<uint16*>(std::realloc(ParticleIndices, sizeof(uint16) * (NewMaxActiveParticles + 1)));
		if (!NewIndices)
		{
			return false;
		}
		ParticleIndices = NewIndices;

		// 새로 생긴 칸은 모두 빈 칸
		for (int32 Index = MaxActiveParticles; Index < NewMaxActiveParticles; ++Index)
		{
			ParticleIndices[Index] = static_cast<uint16>(Index);
		}
		MaxActiveParticles = NewMaxActiveParticles;
	}
	return true;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	const bool bFirstTime = (SecondsSinceCreation <= 0.0f);
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

	const float EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

	if (bEnabled)
	{
		KillParticles();
		ResetParticleParameters(DeltaTime);
		CurrentMaterial = LODLevel->RequiredModule->Material;

		Tick_ModuleUpdate(DeltaTime, LODLevel);
		SpawnFraction = Tick_SpawnParticles(DeltaTime, LODLevel, bSuppressSpawning, bFirstTime);
		Tick_ModulePostUpdate(DeltaTime, LODLevel);

		if (ActiveParticles > 0)
		{
			UpdateBoundingBox(DeltaTime);
		}

		Tick_ModuleFinalUpdate(DeltaTime, LODLevel);
		CheckEmitterFinished();
	}

	EmitterTime += EmitterDelay;
	LastDeltaTime = DeltaTime;
}

float FParticleEmitterInstance::Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	if (Component.GetJustRegistered())
	{
		Location = Component.GetComponentLocation();
		OldLocation = Location;
	}
	else
	{
		OldLocation = Location;
		Location = Component.GetComponentLocation();
	}

	SecondsSinceCreation += DeltaTime;
	EmitterTime += DeltaTime;

	UParticleModuleRequired* RequiredModule = InCurrentLODLevel->RequiredModule;
	const bool bLooped = (EmitterDuration > 0.0f) && (EmitterTime >= EmitterDuration);

	float EmitterDelay = CurrentDelay;
	if (bLooped)
	{
		LoopCount++;
		ResetBurstList();
		EmitterTime -= EmitterDuration;

		if (RequiredModule->bDurationRecalcEachLoop || (RequiredModule->bDelayFirstLoopOnly && LoopCount == 1))
		{
			SetupEmitterDuration();
		}
	}

	if (RequiredModule->bDelayFirstLoopOnly && LoopCount > 0)
	{
		EmitterDelay = 0.0f;
	}

	// 지연 시간 동안은 EmitterTime이 음수가 되어 Spawn이 일어나지 않는다. Tick 끝에서 되돌린다.
	EmitterTime -= EmitterDelay;
	return EmitterDelay;
}

float FParticleEmitterInstance::Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime)
{
	if (!bHaltSpawning && !bSuppressSpawning && EmitterTime >= 0.0f)
	{
		const UParticleModuleRequired* RequiredModule = InCurrentLODLevel->RequiredModule;
		if (RequiredModule->EmitterLoops == 0
			|| LoopCount < RequiredModule->EmitterLoops
			|| SecondsSinceCreation < EmitterDuration * RequiredModule->EmitterLoops
			|| bFirstTime)
		{
			SpawnFraction = Spawn(DeltaTime);
		}
	}
	return SpawnFraction;
}

void FParticleEmitterInstance::Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetLODLevel(0);
	for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ++ModuleIndex)
	{
		UParticleModule* Module = InCurrentLODLevel->UpdateModules[ModuleIndex];
		if (Module && Module->bEnabled && Module->bUpdateModule)
		{
			// payload 위치는 최상위 LOD의 같은 자리 모듈로 찾는다.
			const int32 Offset = static_cast<int32>(GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]));
			Module->Update(UParticleModule::FUpdateContext(*this, Offset, DeltaTime));
		}
	}
}

void FParticleEmitterInstance::Tick_ModulePostUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	if (InCurrentLODLevel->TypeDataModule && InCurrentLODLevel->TypeDataModule->bEnabled)
	{
		InCurrentLODLevel->TypeDataModule->Update(UParticleModule::FUpdateContext(*this, TypeDataOffset, DeltaTime));
	}
}

void FParticleEmitterInstance::Tick_ModuleFinalUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetLODLevel(0);
	for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ++ModuleIndex)
	{
		UParticleModule* Module = InCurrentLODLevel->UpdateModules[ModuleIndex];
		if (Module && Module->bEnabled && Module->bFinalUpdateModule)
		{
			const int32 Offset = static_cast<int32>(GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]));
			Module->FinalUpdate(UParticleModule::FUpdateContext(*this, Offset, DeltaTime));
		}
	}

	UParticleModuleTypeDataBase* TypeData = InCurrentLODLevel->TypeDataModule;
	if (TypeData && TypeData->bEnabled && TypeData->bFinalUpdateModule)
	{
		TypeData->FinalUpdate(UParticleModule::FUpdateContext(*this, TypeDataOffset, DeltaTime));
	}
}

void FParticleEmitterInstance::KillParticles()
{
	// 뒤에서부터 돌아야 swap으로 끌려온 파티클을 건너뛰지 않는다.
	for (int32 Index = ActiveParticles - 1; Index >= 0; --Index)
	{
		const int32 CurrentIndex = ParticleIndices[Index];
		DECLARE_PARTICLE_CONST(Particle, ParticleData + CurrentIndex * ParticleStride);
		if (Particle.RelativeTime > 1.0f)
		{
			ParticleIndices[Index] = ParticleIndices[ActiveParticles - 1];
			ParticleIndices[ActiveParticles - 1] = static_cast<uint16>(CurrentIndex);
			ActiveParticles--;
		}
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < 0 || Index >= ActiveParticles)
	{
		return;
	}
	const uint16 KillIndex = ParticleIndices[Index];
	ParticleIndices[Index] = ParticleIndices[ActiveParticles - 1];
	ParticleIndices[ActiveParticles - 1] = KillIndex;
	ActiveParticles--;
}

void FParticleEmitterInstance::KillParticlesForced()
{
	ActiveParticles = 0;
	ParticleCounter = 0;
}

void FParticleEmitterInstance::ResetParticleParameters(float DeltaTime)
{
	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[Index]);
		Particle.Velocity = Particle.BaseVelocity;
		Particle.Size = Particle.BaseSize;
		Particle.RotationRate = Particle.BaseRotationRate;
		Particle.Color = Particle.BaseColor;

		// 이번 프레임에 생성된 파티클은 PostSpawn에서 이미 일부 시간을 받았다.
		const bool bJustSpawned = (Particle.Flags & STATE_Particle_JustSpawned) != 0;
		if (!bJustSpawned)
		{
			Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
		}
	}
}

void FParticleEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	const UParticleModuleRequired* RequiredModule = GetCurrentLODLevelChecked()->RequiredModule;
	const bool bUseLocalSpace = RequiredModule->bUseLocalSpace;
	const FMatrix ComponentToWorld = Component.GetComponentToWorld();

	FVector MinVal(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxVal(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 Index = 0; Index < ActiveParticles; ++Index)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[Index]);

		Particle.OldLocation = Particle.Location;

		const bool bJustSpawned = (Particle.Flags & STATE_Particle_JustSpawned) != 0;
		Particle.Flags &= ~static_cast<int32>(STATE_Particle_JustSpawned);

		// 위치·회전 적분. 이번 프레임에 생성된 파티클은 PostSpawn에서 이미 움직였다.
		if ((Particle.Flags & STATE_Particle_Freeze) == 0 && !bJustSpawned)
		{
			if ((Particle.Flags & STATE_Particle_FreezeTranslation) == 0)
			{
				Particle.Location += Particle.Velocity * DeltaTime;
			}
			if ((Particle.Flags & STATE_Particle_FreezeRotation) == 0)
			{
				Particle.Rotation = std::fmod(Particle.Rotation + Particle.RotationRate * DeltaTime, TwoPi);
			}
		}

		const FVector WorldLocation = bUseLocalSpace ? ComponentToWorld.TransformPosition(Particle.Location) : Particle.Location;
		const float Extent = FMath::Max(std::abs(Particle.Size.X), FMath::Max(std::abs(Particle.Size.Y), std::abs(Particle.Size.Z)));
		const FVector ExtentVector(Extent, Extent, Extent);
		MinVal = FVector::Min(MinVal, WorldLocation - ExtentVector);
		MaxVal = FVector::Max(MaxVal, WorldLocation + ExtentVector);
	}

	ParticleBoundingBox.Min = MinVal;
	ParticleBoundingBox.Max = MaxVal;
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	UParticleModuleSpawn* SpawnModule = LODLevel->SpawnModule;
	FRandomStream& RandomStream = Component.GetRandomStream();

	float SpawnRate = 0.0f;
	int32 BurstCount = 0;
	const float OldLeftover = SpawnFraction;

	if (SpawnModule && SpawnModule->bEnabled)
	{
		const float RateScale = SpawnModule->RateScale.IsCreated() ? SpawnModule->RateScale.GetValue(EmitterTime, &RandomStream) : 1.0f;
		SpawnRate = FMath::Max(0.0f, SpawnModule->Rate.GetValue(EmitterTime, &RandomStream) * RateScale);
		BurstCount = GetCurrentBurstCount();
	}

	if (SpawnRate <= 0.0f && BurstCount <= 0)
	{
		return SpawnFraction;
	}

	// 지난 프레임에 남은 소수부를 이어서 계산한다.
	float NewLeftover = OldLeftover + DeltaTime * SpawnRate;
	const int32 Number = static_cast<int32>(std::floor(NewLeftover));
	const float Increment = (SpawnRate > 0.0f) ? (1.0f / SpawnRate) : 0.0f;
	const float StartTime = DeltaTime + OldLeftover * Increment - Increment;
	NewLeftover -= static_cast<float>(Number);

	const int32 NewCount = ActiveParticles + Number + BurstCount;
	if (NewCount >= MaxActiveParticles)
	{
		const int32 Extra = static_cast<int32>(std::sqrt(std::sqrt(static_cast<float>(NewCount)))) + 1;
		if (!Resize(NewCount + Extra))
		{
			return OldLeftover;
		}
	}

	const UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;
	const FVector InitialLocation = RequiredModule->bUseLocalSpace ? FVector() : Location;

	SpawnParticles(Number, StartTime, Increment, InitialLocation, FVector());
	SpawnParticles(BurstCount, 0.0f, 0.0f, InitialLocation, FVector());
	return NewLeftover;
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	Count = FMath::Min(Count, MaxActiveParticles - ActiveParticles);
	if (Count <= 0)
	{
		return;
	}

	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->GetLODLevel(0);

	float SpawnTime = StartTime;
	float Interp = 1.0f;
	const float InterpIncrement = (Increment > 0.0f) ? (1.0f / Count) : 0.0f;

	for (int32 SpawnIndex = 0; SpawnIndex < Count; ++SpawnIndex)
	{
		// 빈 칸 목록의 맨 앞 칸을 꺼낸다.
		const uint16 NextFreeIndex = ParticleIndices[ActiveParticles];
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);
		const int32 CurrentParticleIndex = ActiveParticles++;

		PreSpawn(Particle, InitialLocation, InitialVelocity);
		for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ++ModuleIndex)
		{
			UParticleModule* SpawnModule = LODLevel->SpawnModules[ModuleIndex];
			if (SpawnModule && SpawnModule->bEnabled)
			{
				const int32 Offset = static_cast<int32>(GetModuleDataOffset(HighestLODLevel->SpawnModules[ModuleIndex]));
				SpawnModule->Spawn(UParticleModule::FSpawnContext(*this, Offset, SpawnTime, Particle));
			}
		}
		PostSpawn(Particle, Interp, SpawnTime);

		// 생성하자마자 수명이 끝난 파티클은 바로 지운다.
		if (Particle->RelativeTime > 1.0f)
		{
			KillParticle(CurrentParticleIndex);
		}

		SpawnTime -= Increment;
		Interp -= InterpIncrement;
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	// 이전 파티클이 남긴 값을 지운다. payload까지 포함한다.
	std::memset(Particle, 0, ParticleSize);
	Particle->Location = InitialLocation;
	Particle->BaseVelocity = InitialVelocity;
	Particle->Velocity = InitialVelocity;
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime)
{
	const UParticleModuleRequired* RequiredModule = GetCurrentLODLevelChecked()->RequiredModule;

	// 이미터가 이번 프레임에 움직였다면, 생성 시점에 맞춰 이전 위치 쪽으로 당긴다.
	if (!RequiredModule->bUseLocalSpace && (OldLocation - Location).SizeSquared() > 1.0f)
	{
		Particle->Location += (OldLocation - Location) * InterpolationPercentage;
	}

	// 생성 시점부터 이번 프레임 끝까지 이동
	Particle->OldLocation = Particle->Location;
	Particle->Location += Particle->Velocity * SpawnTime;

	Particle->Flags |= static_cast<int32>((ParticleCounter++) & STATE_CounterMask);
	Particle->Flags |= static_cast<int32>(STATE_Particle_JustSpawned);
}

void FParticleEmitterInstance::SetupEmitterDuration()
{
	if (!SpriteTemplate)
	{
		return;
	}

	const int32 LODCount = SpriteTemplate->LODLevels.Num();
	EmitterDurations.SetNum(LODCount);

	FRandomStream& RandomStream = Component.GetRandomStream();
	for (int32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		UParticleLODLevel* LODLevel = SpriteTemplate->LODLevels[LODIndex];
		if (!LODLevel || !LODLevel->RequiredModule)
		{
			EmitterDurations[LODIndex] = 0.0f;
			continue;
		}
		const UParticleModuleRequired* RequiredModule = LODLevel->RequiredModule;

		CurrentDelay = RequiredModule->EmitterDelay + Component.GetEmitterDelay();
		if (RequiredModule->bEmitterDelayUseRange)
		{
			CurrentDelay = RandomStream.FRandRange(RequiredModule->EmitterDelayLow, RequiredModule->EmitterDelay) + Component.GetEmitterDelay();
		}

		float Duration = RequiredModule->EmitterDuration;
		if (RequiredModule->bEmitterDurationUseRange)
		{
			Duration = RandomStream.FRandRange(RequiredModule->EmitterDurationLow, RequiredModule->EmitterDuration);
		}

		// 루프 길이에는 지연 시간이 포함된다.
		EmitterDurations[LODIndex] = Duration + CurrentDelay;

		// 첫 루프만 지연한다면, 두 번째 루프부터는 지연을 뺀다.
		if (LoopCount == 1 && RequiredModule->bDelayFirstLoopOnly
			&& (RequiredModule->EmitterLoops == 0 || RequiredModule->EmitterLoops > 1))
		{
			EmitterDurations[LODIndex] -= CurrentDelay;
		}
	}

	if (CurrentLODLevelIndex < EmitterDurations.Num())
	{
		EmitterDuration = EmitterDurations[CurrentLODLevelIndex];
	}
}

int32 FParticleEmitterInstance::GetCurrentBurstCount()
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	UParticleModuleSpawn* SpawnModule = LODLevel->SpawnModule;
	if (!SpawnModule || SpawnModule->BurstList.Num() == 0 || CurrentLODLevelIndex >= BurstFired.Num())
	{
		return 0;
	}

	FRandomStream& RandomStream = Component.GetRandomStream();
	TArray<uint8>& Fired = BurstFired[CurrentLODLevelIndex];

	int32 Total = 0;
	for (int32 BurstIndex = 0; BurstIndex < SpawnModule->BurstList.Num() && BurstIndex < Fired.Num(); ++BurstIndex)
	{
		const FParticleBurst& Burst = SpawnModule->BurstList[BurstIndex];
		if (Fired[BurstIndex] || EmitterTime < Burst.Time)
		{
			continue;
		}

		int32 Count = Burst.Count;
		if (Burst.CountLow > -1)
		{
			Count = Burst.CountLow + static_cast<int32>(RandomStream.GetFraction() * (Burst.Count - Burst.CountLow + 1));
		}
		const float Scale = SpawnModule->BurstScale.IsCreated() ? SpawnModule->BurstScale.GetValue(EmitterTime, &RandomStream) : 1.0f;
		Total += static_cast<int32>(std::ceil(Count * Scale));
		Fired[BurstIndex] = 1;
	}
	return Total;
}

void FParticleEmitterInstance::ResetBurstList()
{
	for (TArray<uint8>& Fired : BurstFired)
	{
		for (int32 Index = 0; Index < Fired.Num(); ++Index)
		{
			Fired[Index] = 0;
		}
	}
}

void FParticleEmitterInstance::CheckEmitterFinished()
{
	if (ActiveParticles == 0 && HasCompleted())
	{
		bEmitterIsDone = true;
	}
}

bool FParticleEmitterInstance::HasCompleted() const
{
	if (!SpriteTemplate)
	{
		return true;
	}

	const UParticleModuleRequired* RequiredModule = GetCurrentLODLevelChecked()->RequiredModule;
	if (RequiredModule->EmitterLoops == 0 || SecondsSinceCreation < EmitterDuration * RequiredModule->EmitterLoops)
	{
		return false;
	}
	return ActiveParticles == 0;
}

void FParticleEmitterInstance::Rewind()
{
	SecondsSinceCreation = 0.0f;
	EmitterTime = 0.0f;
	LoopCount = 0;
	ParticleCounter = 0;
	bEnabled = true;
	bEmitterIsDone = false;
	ResetBurstList();
}

void FParticleEmitterInstance::SetCurrentLODIndex(int32 InLODIndex)
{
	if (!SpriteTemplate || InLODIndex < 0 || InLODIndex >= SpriteTemplate->LODLevels.Num())
	{
		return;
	}
	CurrentLODLevelIndex = InLODIndex;
	CurrentLODLevel = SpriteTemplate->LODLevels[InLODIndex];
	if (InLODIndex < EmitterDurations.Num())
	{
		EmitterDuration = EmitterDurations[InLODIndex];
	}
}

UParticleLODLevel* FParticleEmitterInstance::GetCurrentLODLevelChecked() const
{
	assert(CurrentLODLevel && CurrentLODLevel->RequiredModule);
	return CurrentLODLevel;
}

uint32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module) const
{
	const uint32* Offset = SpriteTemplate->ModuleOffsetMap.Find(Module);
	return Offset ? *Offset : 0;
}

uint8* FParticleEmitterInstance::GetModuleInstanceData(UParticleModule* Module) const
{
	if (!InstanceData)
	{
		return nullptr;
	}
	const uint32* Offset = SpriteTemplate->ModuleInstanceOffsetMap.Find(Module);
	return Offset ? InstanceData + *Offset : nullptr;
}
