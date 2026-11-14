#include "EnginePCH.h"
#include "ParticleModuleSpawn.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleSpawn::InitializeDefaults()
{
	if (!Rate)
	{
		Rate = CreateFloatConstant(10.0f);
	}
}

float UParticleModuleSpawn::GetRate(FParticleEmitterInstance& Owner) const
{
	return Evaluate(Rate, RateCache, Owner.EmitterTime);
}