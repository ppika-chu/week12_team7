#include "EnginePCH.h"
#include "Particles/Rotation/ParticleModuleRotation.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include <cmath>

UParticleModuleRotation::UParticleModuleRotation()
{
	bSpawnModule = true;
	StartRotation.SetDistribution(CreateDefaultSubobject<UDistributionFloatConstant>("DistributionStartRotation"));
	StartRotationRate.SetDistribution(CreateDefaultSubobject<UDistributionFloatConstant>("DistributionStartRotationRate"));
}

void UParticleModuleRotation::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	FParticleEmitterInstance& Owner = Context.Owner;
	FRandomStream& RandomStream = Owner.GetRandomStream();
	const float Angle = FMath::DegreesToRadians(StartRotation.GetValue(Owner.EmitterTime, &RandomStream));
	const float Rate = FMath::DegreesToRadians(StartRotationRate.GetValue(Owner.EmitterTime, &RandomStream));

	// 생성 후 이번 틱에 흐른 시간만큼 회전 보정
	Particle.Rotation = std::fmod(Particle.Rotation + Angle + Rate * Context.SpawnTime, 2.0f * PI);
	Particle.BaseRotationRate += Rate;
	Particle.RotationRate += Rate;
}
