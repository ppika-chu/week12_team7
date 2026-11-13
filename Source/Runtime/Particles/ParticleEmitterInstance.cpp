#include "EnginePCH.h"
#include "ParticleEmitterInstance.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "Core/HAL/UnrealMemory.h"
#include <algorithm>
#include <cassert>
#include <limits>
#include <new>

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
	return true;
}

// 빈 슬롯 초기화와 기본 상태를 적용한 파티클 생성
int32 FParticleEmitterInstance::SpawnParticles(int32 Count, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (!CurrentLODLevel || !CurrentLODLevel->bEnabled || Count <= 0)
	{
		return 0;
	}

	const int32 SpawnCount = std::min(Count, MaxActiveParticles - ActiveParticles);
	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		// 빈 구간의 첫 인덱스에 대응하는 물리 슬롯 선택
		const uint16 Slot = DataContainer.ParticleIndices[ActiveParticles];
		uint8* Address = DataContainer.ParticleData + static_cast<uint64>(Slot) * ParticleStride;

		// 이전 payload 제거와 기본 파티클 객체 생성
		FMemory::Memzero(Address, ParticleStride);
		FBaseParticle* Particle = new (Address) FBaseParticle{};

		// 생성 인자 적용과 활성 구간 편입
		Particle->Location = Particle->OldLocation = InitialLocation;
		Particle->Velocity = Particle->BaseVelocity = InitialVelocity;
		Particle->ParticleId = ++ParticleCounter;
		++ActiveParticles;
	}
	return SpawnCount;
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