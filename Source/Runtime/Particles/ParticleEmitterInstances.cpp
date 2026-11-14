#include "EnginePCH.h"
#include "ParticleEmitterInstances.h"
#include "ParticleEmitter.h"
#include "ParticleHelper.h"
#include "ParticleModule.h"
#include "ParticleModuleRequired.h"
#include "ParticleModuleSpawn.h"
#include "ParticleLODLevel.h"
#include "Core/HAL/UnrealMemory.h"
#include "Components/ParticleSystemComponent.h"

FParticleEmitterInstance::FParticleEmitterInstance(UParticleSystemComponent& InComponent)
	: SpriteTemplate(NULL)
	, Component(&InComponent)
	, CurrentLODLevelIndex(0)
	, CurrentLODLevel(NULL)
	, bEnabled(true)
	, ParticleData(NULL)
	, ParticleIndices(NULL)
	, InstanceData(NULL)
	, InstancePayloadSize(0)
	, PayloadOffset(0)
	, OldLocation(0.f, 0.f, 0.f)
	, Location(0.f, 0.f, 0.f)
	, ParticleSize(0)
	, ParticleStride(0)
	, ActiveParticles(0)
	, ParticleCounter(0)
	, MaxActiveParticles(0)
	, SpawnFraction(0.f)
	, SecondsSinceCreation(0.f)
	, LoopCount(0)
	, EmitterDuration(0.f)
	, EmitterTime(0.f)
	, CurrentDelay(0.f)
	, CurrentMaterial(NULL)
{

}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	//블록 할당 해제
	FMemory::Free(ParticleData);
	FMemory::Free(ParticleIndices);
	FMemory::Free(InstanceData);
}

void FParticleEmitterInstance::InitParameters(UParticleEmitter* Emitter)
{
	SpriteTemplate = Emitter;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate->LODLevels[0];
}

void FParticleEmitterInstance::Init()
{
	//슬롯 크기 구하기
	//1. Emitter 크기부터 시작
	ParticleSize = SpriteTemplate->ParticleSize;

	//여기서부터 Payload
	PayloadOffset = ParticleSize;
	ParticleSize = (int32)FMemory::QuantizeSize(ParticleSize, 16);
	ParticleStride = ParticleSize;

	ActiveParticles = 0;
	ParticleCounter = 0;

	//기본은 10개 슬롯
	Resize(10);
}

bool FParticleEmitterInstance::Resize(int32 NewMax)
{
	if (NewMax > 65535)
	{
		return false; // 인덱스가 uint16
	}
	if (NewMax <= MaxActiveParticles)
	{
		return true;
	}

	ParticleData = (uint8*)FMemory::Realloc(ParticleData, ParticleStride * NewMax);
	ParticleIndices = (uint16*)FMemory::Realloc(ParticleIndices, sizeof(uint16) * (NewMax + 1));

	for (int32 i = MaxActiveParticles; i < NewMax; ++i)
	{
		ParticleIndices[i] = i; // 새 칸만 1:1로 채움
	}

	MaxActiveParticles = NewMax;
	return true;
}

UParticleLODLevel* FParticleEmitterInstance::GetCurrentLODLevelChecked()
{
	assert(SpriteTemplate != NULL);
	assert(CurrentLODLevel != NULL);
	assert(CurrentLODLevel->RequiredModule != NULL);
	return CurrentLODLevel;
}

/*
 *	Tick the instance.
 *
 *	@param	DeltaTime			The time slice to use
 *	@param	bSuppressSpawning	If true, do not spawn during Tick
*/
void FParticleEmitterInstance::Tick(float DeltaTime, bool bSupressSpawning)
{
	assert(SpriteTemplate);

	bool bFirstTime = SecondsSinceCreation > 0.f ? false : true;

	// Grab the current LOD level
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

	// Handle EmitterTime setup, looping, etc.
	float EmitterDelay = Tick_EmitterTimeSetup(DeltaTime, LODLevel);

	if (bEnabled)
	{
		KillParticles();
		ResetParticleParameters(DeltaTime);

		CurrentMaterial = LODLevel->RequiredModule->Material;
		Tick_ModuleUpdate(DeltaTime, LODLevel);
		
		SpawnFraction = Tick_SpawnParticles(DeltaTime, LODLevel, bSupressSpawning, bFirstTime);

		if (ActiveParticles > 0)
		{
			//UpdateOrbitData(DeltaTime)
			UpdateBoundingBox(DeltaTime);
		}

		//Tick_ModuleFinalUpdate(DeltaTime, LODLevel);
		//CheckEmitterFinished();

		IsRenderDataDirty = true;
	}

	EmitterTime += EmitterDelay;
	//LastDeltaTime = DeltaTime;
	//PositionOffsetThisTick = FVector::ZeroVector;
}

//이미터 위치를 기록하고, 이미터 시간을 올리고, 한 바퀴가 끝났는지 봅니다.
float FParticleEmitterInstance::Tick_EmitterTimeSetup(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	OldLocation = Location;
	Location = Component->GetWorldLocation();

	SecondsSinceCreation += DeltaTime;
	EmitterTime += DeltaTime;
	bool bLooped = (EmitterDuration > 0.0f) && (EmitterTime >= EmitterDuration);

	float EmitterDelay = CurrentDelay;

	if (bLooped)
	{
		LoopCount++;
		EmitterTime -= EmitterDuration;
	}

	//Delay 시간이라면 EmitterTime이 음수가 되어, Tick_SpawnParticles에서 >= 0.0f 검사로 스폰되지 않는다.
	//후에 Tick 끝에서 다시 Delay를 더해 값을 복구한다.
	EmitterTime -= EmitterDelay;
	return EmitterDelay;
}

//죽은 파티클 제거, active array에서 제거한다.
void FParticleEmitterInstance::KillParticles()
{
	if (ActiveParticles > 0)
	{
		for (int i = ActiveParticles - 1; i >= 0; --i)
		{
			KillParticles(i);
		}
	}
}

void FParticleEmitterInstance::KillParticles(int32 Index)
{
	const int32 CurrentIndex = ParticleIndices[Index];
	const uint8* ParticleBase = ParticleData + CurrentIndex * ParticleStride;
	FBaseParticle& Particle = *((FBaseParticle*)ParticleBase);

	if (Particle.RelativeTime > 1.0f)
	{
		ParticleIndices[Index] = ParticleIndices[ActiveParticles - 1];
		ParticleIndices[ActiveParticles - 1] = CurrentIndex;
		ActiveParticles--;
	}
}

//파티클 값을 초기화 + 파티클 시간 진행
//“Over Life” 계열 업데이트 모듈 때문에 기본값으로 바꿔야 한다.
void FParticleEmitterInstance::ResetParticleParameters(float DeltaTime)
{
	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; ParticleIndex++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
		Particle.Velocity = Particle.BaseVelocity;
		Particle.Size = Particle.BaseSize; // 원본은 GetParticleBaseSize(Particle)
		Particle.RotationRate = Particle.BaseRotationRate;
		Particle.Color = Particle.BaseColor;

		//시간 진행
		Particle.RelativeTime += Particle.OneOverMaxLifetime * DeltaTime;
	}
}

///UpdateModule들 Update 호출
void FParticleEmitterInstance::Tick_ModuleUpdate(float DeltaTime, UParticleLODLevel* InCurrentLODLevel)
{
	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
	assert(HighestLODLevel);
	for (int32 ModuleIndex = 0; ModuleIndex < InCurrentLODLevel->UpdateModules.Num(); ModuleIndex++)
	{
		UParticleModule* CurrentModule = InCurrentLODLevel->UpdateModules[ModuleIndex];
		if (CurrentModule && CurrentModule->bEnabled && CurrentModule->bUpdateModule)
		{
			CurrentModule->Update({ *this, (int32)GetModuleDataOffset(HighestLODLevel->UpdateModules[ModuleIndex]), DeltaTime });
		}
	}
}

//추가 갯수만큼 파티클 스폰 & SpawnModuele들 Spawn 호출
float FParticleEmitterInstance::Tick_SpawnParticles(float DeltaTime, UParticleLODLevel* InCurrentLODLevel, bool bSuppressSpawning, bool bFirstTime)
{
	if (!bSuppressSpawning && (EmitterTime >= 0.f))
	{
		// 무한 반복이거나, 루프 횟수가 남았거나
		// 생성 후 지난 시간이 EmitterDuration * EmitterLoops 보다 작거나
		// 첫 번째 호출이면 스폰 호출
		if ((InCurrentLODLevel->RequiredModule->EmitterLoops == 0) ||
			(LoopCount < InCurrentLODLevel->RequiredModule->EmitterLoops) ||
			(SecondsSinceCreation < (EmitterDuration * InCurrentLODLevel->RequiredModule->EmitterLoops)) ||
			bFirstTime)
		{
			bFirstTime = false;
			SpawnFraction = Spawn(DeltaTime);
		}
	}

	return SpawnFraction;
}

//개수 계산, 용량 확보, SpawnParticles 호출
float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();

	float SpawnRate = 0.0f;
	float OldLeftover = SpawnFraction;

	float RateScale = LODLevel->SpawnModule->RateScale;
	SpawnRate += LODLevel->SpawnModule->Rate * RateScale;
	SpawnRate = FMath::Max(0.0f, SpawnRate);

	if (SpawnRate > 0.f)
	{
		float SafetyLeftover = OldLeftover;
		float NewLeftover = SafetyLeftover + SpawnRate * DeltaTime;
		int32 Number = FMath::FloorToInt(NewLeftover);

		/*Increment = 파티클 사이의 시간 간격, ex) SpawnRate = 20이면, 0.05초
			StartTime = 첫 파티클이 틱 끝 기준으로 얼마 전에 태어났어야 하는지
			다음 파티클이 태어나는 시점(틱 시작 기준) = Increment - OldLeftover * Increment
			그 시점부터 틱 끝까지 남은 시간 = DeltaTime - (Increment - OldLeftover * Increment)
			= DeltaTime + OldLeftover* Increment - Increment
		*/
		float Increment = (SpawnRate > 0.0f) ? (1.f / SpawnRate) : 0.f;
		float StartTime = DeltaTime + OldLeftover * Increment - Increment;
		NewLeftover = NewLeftover - Number;

		bool bProcessSpawn = true;
		int32 NewCount = ActiveParticles + Number;

		if (NewCount > ActiveParticles)
		{
			//NewCount의 네제곱근만큼 여유분을 더 더한다,
			bProcessSpawn = Resize(NewCount + FMath::TruncToInt(FMath::Sqrt(FMath::Sqrt((float)NewCount)) + 1));
		}

		if (bProcessSpawn == true)
		{
			const FVector InitialLocation = Location;
			//TODO : 위치, 속도 모듈 추가되면 여기에 넘기기
			SpawnParticles(Number, StartTime, Increment, InitialLocation, FVector::ZeroVector, NULL);
			return NewLeftover;
		}

		//Resize가 실패하면 SafetyLeftover(원래 값)를 돌려줘서, 쌓인 소수 부분을 잃지 않고 다음 프레임에 다시 시도
		return SafetyLeftover;
	}

	return SpawnFraction;
}

//파티클 스폰, 스폰 모듈 호출
void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, FParticleEventInstancePayload* EventPayload)
{
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	assert(ActiveParticles <= MaxActiveParticles);

	Count = FMath::Min<int32>(Count, MaxActiveParticles - ActiveParticles); // 할당 범위를 넘지 않게

	UParticleLODLevel* HighestLODLevel = SpriteTemplate->LODLevels[0];
	float SpawnTime = StartTime;
	float Interp = 1.0f;
	const float InterpIncrement = (Count > 0 && Increment > 0.0f) ? (1.0f / (float)Count) : 0.0f;

	for (int32 i = 0; i < Count; i++)
	{
		//다음 빈 슬롯 메모리 주소를 가져와 FBaseParticle로 취급
		uint16 NextFreeIndex = ParticleIndices[ActiveParticles];
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleStride * NextFreeIndex);
		const uint32 CurrentParticleIndex = ActiveParticles++;

		PreSpawn(Particle, InitialLocation, InitialVelocity);
		for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
		{
			UParticleModule* SpawnModule = LODLevel->SpawnModules[ModuleIndex];
			if (SpawnModule->bEnabled)
			{
				UParticleModule* OffsetModule = HighestLODLevel->SpawnModules[ModuleIndex];
				SpawnModule->Spawn({ *this, (int32)GetModuleDataOffset(OffsetModule), SpawnTime, Particle });
			}
		}
		PostSpawn(Particle, Interp, SpawnTime);

		// 스폰 모듈이 RelativeTime을 1보다 크게 두면 "만들지 말라"는 표시다
		if (Particle->RelativeTime > 1.0f)
		{
			KillParticles(CurrentParticleIndex);
			continue;
		}

		SpawnTime -= Increment;
		Interp -= InterpIncrement;
	}
}

//슬롯을 0으로 초기화, Location, BaseVelocity, Velocity를 채움
void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	assert(Particle);
	assert(ParticleSize > 0);

	FMemory::Memzero(Particle, ParticleSize); // 슬롯 전체를 0으로

	Particle->Location = InitialLocation;
	Particle->BaseVelocity = InitialVelocity;
	Particle->Velocity = InitialVelocity;
}

//파티클 이동
void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float InterpolationPercentage, float SpawnTime)
{
	// 월드 공간이면, 이미터가 이번 프레임에 움직인 경로 위에 파티클을 펼친다
	UParticleLODLevel* LODLevel = GetCurrentLODLevelChecked();
	if (LODLevel->RequiredModule->bUseLocalSpace == false)
	{
		if ((OldLocation - Location).SizeSquared() > 1.f)
		{
			Particle->Location += InterpolationPercentage * (OldLocation - Location);
		}
	}

	Particle->OldLocation = Particle->Location;
	Particle->Location += SpawnTime * Particle->Velocity; // 태어난 뒤 흐른 시간만큼 이동

	Particle->Flags |= ((ParticleCounter++) & STATE_CounterMask);
	Particle->Flags |= STATE_Particle_JustSpawned;
}

//이미터의 BoundingBox를 업데이트한다.
void FParticleEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	for (int32 i = 0; i < ActiveParticles; i++)
	{
		DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[i]);

		Particle.OldLocation = Particle.Location;

		bool bJustSpawned = (Particle.Flags & STATE_Particle_JustSpawned) != 0;
		Particle.Flags &= ~STATE_Particle_JustSpawned;

		// 방금 태어난 파티클은 PostSpawn에서 이미 부분 이동을 했으므로 건너뛴다
		if (!bJustSpawned)
		{
			Particle.Location = Particle.Location + DeltaTime * Particle.Velocity;
			Particle.Rotation = FMath::Fmod((DeltaTime * Particle.RotationRate) + Particle.Rotation, 2.f * PI);
		}
	}

	//TODO : 이동한 위치를 바탕으로 BoundingBox 갱신, 나중에 컬링 구현에 사용
}

//인스턴스 슬롯에서 Module의 오프셋을 구한다.
int32 FParticleEmitterInstance::GetModuleDataOffset(UParticleModule* Module)
{
	assert(SpriteTemplate);

	int32* Offset = SpriteTemplate->ModuleOffsetMap.Find(Module);
	return (Offset != nullptr) ? *Offset : 0;
}
