#include "EnginePCH.h"
#include "ParticleModuleVelocity.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleVelocity::InitializeDefaults()
{
	if (!StartVelocity)
	{
		StartVelocity = CreateVectorConstant(FVector::ZeroVector);
	}
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const
{
	Particle.BaseVelocity += Evaluate(StartVelocity, StartVelocityCache, Owner.EmitterTime);
	Particle.Velocity = Particle.BaseVelocity;
}