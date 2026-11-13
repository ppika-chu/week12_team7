#pragma once
#include "EnginePCH.h"

#include "UObject/Object.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;

UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	void Build();
	void CacheEmitterModuleInfo();
	TArray<UParticleLODLevel*> LODLevels;

	//이미터 크기
	int32 ParticleSize;

	//===============
	//offset
	int32 TypeDataOffset;
	TMap<UParticleModule*, int32> ModuleOffsetMap;
};