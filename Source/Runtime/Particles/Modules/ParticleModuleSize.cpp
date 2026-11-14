#include "EnginePCH.h"
#include "ParticleModuleSize.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleSize::InitializeDefaults()
{
	if (!StartSize)
	{
		StartSize = CreateVectorConstant(FVector(1.0f));
	}
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const
{
	Particle.BaseSize = Particle.Size = Evaluate(StartSize, StartSizeCache, Owner.EmitterTime);
}