#include "EnginePCH.h"
#include "ParticleModuleLocation.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleLocation::InitializeDefaults()
{
	if (!StartLocation)
	{
		StartLocation = CreateVectorConstant(FVector::ZeroVector);
	}
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const
{
	Particle.Location += Evaluate(StartLocation, StartLocationCache, Owner.EmitterTime);
	Particle.OldLocation = Particle.Location;
}