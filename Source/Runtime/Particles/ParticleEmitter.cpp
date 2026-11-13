#include "EnginePCH.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"

// 새 LOD의 생성과 배열 순번 부여
UParticleLODLevel* UParticleEmitter::AddLODLevel()
{
	UParticleLODLevel* NewLODLevel = NewObject<UParticleLODLevel>(this);
	NewLODLevel->Level = LODLevels.Num();
	LODLevels.Add(NewLODLevel);

	return NewLODLevel;
}