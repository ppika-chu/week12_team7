#include "EnginePCH.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"

UParticleLODLevel* UParticleEmitter::AddLODLevel()
{
	UParticleLODLevel* NewLODLevel = NewObject<UParticleLODLevel>(this);
	NewLODLevel->Level = LODLevels.Num();
	NewLODLevel->RequiredModule = NewObject<UParticleModuleRequired>(NewLODLevel);
	NewLODLevel->SpawnModule = NewObject<UParticleModuleSpawn>(NewLODLevel);
	NewLODLevel->SpawnModule->InitializeDefaults();
	LODLevels.Add(NewLODLevel);
	return NewLODLevel;
}