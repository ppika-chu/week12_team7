#include "EnginePCH.h"
#include "Particles/Location/ParticleModuleLocation.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

UParticleModuleLocation::UParticleModuleLocation()
{
	bSpawnModule = true;

	StartLocation.Distribution = CreateDefaultSubobject<UDistributionVectorUniform>("DistributionStartLocation");
}

void UParticleModuleLocation::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	FParticleEmitterInstance& Owner = Context.Owner;

	FVector LocationOffset = StartLocation.GetValue(Owner.EmitterTime, &Owner.GetRandomStream());

	// 월드 공간 시뮬레이션이면 오프셋을 이미터 방향으로 돌린다.
	if (!Owner.GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace)
	{
		LocationOffset = Owner.Component.GetComponentToWorld().TransformVectorNoScale(LocationOffset);
	}

	Particle.Location += LocationOffset;
}
