#include "EnginePCH.h"
#include "ParticleEmitterInstances.h"
#include "ParticleEmitter.h"
#include "Core/HAL/UnrealMemory.h"
#include "Components/ParticleSystemComponent.h"

FParticleEmitterInstance::FParticleEmitterInstance(UParticleSystemComponent& InComponent)
	: SpriteTemplate(NULL), 
	Component(&InComponent)
	, CurrentLODLevelIndex(0)
	, CurrentLODLevel(NULL)
	, ParticleData(NULL)
	, ParticleIndices(NULL)
	, InstanceData(NULL)
	, InstancePayloadSize(0)
	, PayloadOffset(0)
	, ParticleSize(0)
	, ParticleStride(0)
	, ActiveParticles(0)
	, ParticleCounter(0)
	, MaxActiveParticles(0)

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
