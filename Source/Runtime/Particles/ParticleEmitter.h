#pragma once
#include "EnginePCH.h"

#include "UObject/Object.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;
struct FParticleEmitterInstance;

//~=============================================================================
//	Burst emissions
//~=============================================================================
enum EParticleBurstMethod : int
{
	EPBM_Instant,
	EPBM_Interpolated,
	EPBM_MAX,
};


UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	void Build();
	void CacheEmitterModuleInfo();
	UParticleLODLevel* GetCurrentLODLevel(FParticleEmitterInstance* Instance);
	TArray<UParticleLODLevel*> LODLevels;

	//이미터 크기
	int32 ParticleSize;

	//===============
	//offset
	int32 TypeDataOffset;
	TMap<UParticleModule*, int32> ModuleOffsetMap;
};