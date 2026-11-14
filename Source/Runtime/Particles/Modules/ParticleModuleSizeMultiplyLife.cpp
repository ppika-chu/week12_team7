#include "EnginePCH.h"
#include "ParticleModuleSizeMultiplyLife.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleSizeMultiplyLife::InitializeDefaults()
{
	if (!LifeMultiplier)
	{
		LifeMultiplier = CreateVectorConstant(FVector(1.0f));
	}
}

void UParticleModuleSizeMultiplyLife::Update(FParticleEmitterInstance&, FBaseParticle& Particle, float) const
{
	Particle.Size *= Evaluate(LifeMultiplier, LifeMultiplierCache, Particle.RelativeTime);
}