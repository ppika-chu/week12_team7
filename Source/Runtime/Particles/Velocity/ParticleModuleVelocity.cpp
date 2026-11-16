#include "EnginePCH.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"

UParticleModuleVelocity::UParticleModuleVelocity()
{
	bSpawnModule = true;

	StartVelocity.Distribution = CreateDefaultSubobject<UDistributionVectorUniform>("DistributionStartVelocity");
	StartVelocityRadial.Distribution = CreateDefaultSubobject<UDistributionFloatUniform>("DistributionStartVelocityRadial");
}

void UParticleModuleVelocity::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	FParticleEmitterInstance& Owner = Context.Owner;
	FRandomStream& RandomStream = Owner.GetRandomStream();

	const bool bUseLocalSpace = Owner.GetCurrentLODLevelChecked()->RequiredModule->bUseLocalSpace;
	const FMatrix ComponentToWorld = Owner.Component.GetComponentToWorld();

	FVector Velocity = StartVelocity.GetValue(Owner.EmitterTime, &RandomStream);
	if (bUseLocalSpace)
	{
		// 로컬 공간인데 월드 축 기준 속도라면 이미터 회전을 되돌린다.
		if (bInWorldSpace)
		{
			Velocity = ComponentToWorld.GetTransposed().TransformVectorNoScale(Velocity);
		}
	}
	else if (!bInWorldSpace)
	{
		Velocity = ComponentToWorld.TransformVectorNoScale(Velocity);
	}

	// 이미터 중심에서 파티클 쪽으로 퍼지는 속도
	const FVector EmitterOrigin = bUseLocalSpace ? FVector() : Owner.Location;
	const FVector FromOrigin = (Particle.Location - EmitterOrigin).GetSafeNormal();
	Velocity += FromOrigin * StartVelocityRadial.GetValue(Owner.EmitterTime, &RandomStream);

	Particle.Velocity += Velocity;
	Particle.BaseVelocity += Velocity;
}
