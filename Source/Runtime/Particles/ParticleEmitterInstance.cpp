#include "EnginePCH.h"
#include "ParticleEmitterInstance.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "Core/HAL/UnrealMemory.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <new>
#include <cmath>

// 기본 LOD와 정렬된 슬롯 배치의 확정
bool FParticleEmitterInstance::Init(UParticleEmitter* InTemplate, int32 InMaxParticles, int32 InPayloadBytes)
{
	if (!InTemplate || InTemplate->GetLODLevels().IsEmpty() || InPayloadBytes < 0)
	{
		return false;
	}

	// 기본 파티클과 payload 사이의 정렬 여백 계산
	const uint32 Alignment = FParticleDataContainer::Alignment;
	const uint64 NewPayloadOffset = FMemory::QuantizeSize(sizeof(FBaseParticle), Alignment);
	const uint64 NewParticleSize = NewPayloadOffset + static_cast<uint64>(InPayloadBytes);

	// 다음 슬롯 시작 위치를 위한 뒤 정렬 여백 계산
	const uint64 NewStride = FMemory::QuantizeSize(NewParticleSize, Alignment);
	if (NewStride > static_cast<uint64>(std::numeric_limits<int32>::max()) ||
		!DataContainer.Allocate(InMaxParticles, static_cast<int32>(NewStride)))
	{
		return false;
	}

	// 할당 성공 후 새 실행 상태 적용
	SpriteTemplate = InTemplate;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = InTemplate->GetLODLevels()[0];
	PayloadOffset = static_cast<int32>(NewPayloadOffset);
	ParticleSize = static_cast<int32>(NewParticleSize);
	ParticleStride = static_cast<int32>(NewStride);
	MaxActiveParticles = InMaxParticles;
	ActiveParticles = 0;
	ParticleCounter = 0;
	EmitterTime = 0.0f;
	SpawnFraction = 0.0;
	return true;
}

void FParticleEmitterInstance::Tick(float DeltaTime, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!CurrentLODLevel || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	UpdateParticles(DeltaTime);
	EmitterTime += DeltaTime;
	const UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;
	if (!CurrentLODLevel->bEnabled || !SpawnModule || !SpawnModule->bEnabled)
	{
		return;
	}

	const float Rate = SpawnModule->GetRate(*this);
	if (!std::isfinite(Rate) || Rate <= 0.0f)
	{
		return;
	}

	double WholeParticles = 0.0;
	const double FrameFraction = std::modf(static_cast<double>(Rate) * DeltaTime, &WholeParticles);
	SpawnFraction += FrameFraction;
	if (SpawnFraction >= 1.0)
	{
		SpawnFraction -= 1.0;
		WholeParticles += 1.0;
	}

	// 용량을 넘긴 정수 생성량은 대기 없이 폐기
	const int32 SpawnCount = static_cast<int32>(std::min(WholeParticles, static_cast<double>(MaxActiveParticles - ActiveParticles)));
	if (SpawnCount == 0)
	{
		return;
	}

	// 최근 SpawnCount개의 생성 시점 선택
	const double Increment = 1.0 / static_cast<double>(Rate);
	const double StartTime = std::min(static_cast<double>(DeltaTime),
		(static_cast<double>(SpawnCount - 1) + SpawnFraction) * Increment);
	SpawnParticles(SpawnCount, InitialLocation, InitialVelocity, StartTime, Increment);
}

// 빈 슬롯 초기화와 기본 상태를 적용한 파티클 생성
int32 FParticleEmitterInstance::SpawnParticles(int32 Count, const FVector& InitialLocation,
	const FVector& InitialVelocity, double StartTime, double Increment)
{
	if (!CurrentLODLevel || !CurrentLODLevel->bEnabled || Count <= 0 ||
		!std::isfinite(StartTime) || StartTime < 0.0 ||
		StartTime > static_cast<double>(std::numeric_limits<float>::max()) ||
		!std::isfinite(Increment) || Increment < 0.0)
	{
		return 0;
	}

	const int32 SpawnCount = std::min(Count, MaxActiveParticles - ActiveParticles);
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		const uint16 Slot = DataContainer.ParticleIndices[ActiveParticles];
		uint8* Address = DataContainer.ParticleData + static_cast<uint64>(Slot) * ParticleStride;

		FMemory::Memzero(Address, ParticleStride);
		FBaseParticle* Particle = new (Address) FBaseParticle{};

		Particle->Location = Particle->OldLocation = InitialLocation;
		Particle->Velocity = Particle->BaseVelocity = InitialVelocity;
		Particle->ParticleId = ++ParticleCounter;

		for (const UParticleModule* Module : CurrentLODLevel->GetModules())
		{
			if (Module->bEnabled)
			{
				Module->Spawn(*this, *Particle);
			}
		}

		// 프레임 끝까지 살아 있었어야 하는 시간
		const float SpawnTime = static_cast<float>(std::max(0.0, StartTime - static_cast<double>(Index) * Increment));
		if (SpawnTime > 0.0f)
		{
			Particle->RelativeTime += SpawnTime * Particle->OneOverMaxLifetime;
		}
		if (Particle->RelativeTime >= 1.0f)
		{
			continue;
		}

		Particle->OldLocation = Particle->Location;
		for (const UParticleModule* Module : CurrentLODLevel->GetModules())
		{
			if (Module->bEnabled)
			{
				Module->Update(*this, *Particle, SpawnTime);
			}
		}
		Particle->Location += Particle->Velocity * SpawnTime;
		++ActiveParticles;
	}
	return SpawnCount;
}

void FParticleEmitterInstance::UpdateParticles(float DeltaTime)
{
	if (!CurrentLODLevel || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		return;
	}

	BEGIN_UPDATE_LOOP(this)
	Particle.RelativeTime += DeltaTime * Particle.OneOverMaxLifetime;
	if (Particle.RelativeTime >= 1.0f)
	{
		KillParticle(ParticleIndex);
		continue;
	}

	Particle.OldLocation = Particle.Location;
	Particle.Velocity = Particle.BaseVelocity;
	Particle.Size = Particle.BaseSize;
	Particle.Color = Particle.BaseColor;

	for (const UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (Module->bEnabled)
		{
			Module->Update(*this, Particle, DeltaTime);
		}
	}

	Particle.Location += Particle.Velocity * DeltaTime;
	END_UPDATE_LOOP
}

// 본체 이동 없이 마지막 활성 인덱스와 교환하는 제거
bool FParticleEmitterInstance::KillParticle(int32 ActiveIndex)
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles)
	{
		return false;
	}

	--ActiveParticles;
	std::swap(DataContainer.ParticleIndices[ActiveIndex], DataContainer.ParticleIndices[ActiveParticles]);
	return true;
}

// 활성 배열 위치 검증과 물리 슬롯 주소 계산
uint8* FParticleEmitterInstance::GetParticleAddress(int32 ActiveIndex) const
{
	assert(ActiveIndex >= 0 && ActiveIndex < ActiveParticles);
	return DataContainer.ParticleData +
		static_cast<uint64>(DataContainer.ParticleIndices[ActiveIndex]) * ParticleStride;
}