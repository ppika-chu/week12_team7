#include "EnginePCH.h"
#include "ParticleModuleColor.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleColor::InitializeDefaults()
{
	if (!StartColor)
	{
		StartColor = CreateVectorConstant(FVector(1.0f));
	}
	if (!StartAlpha)
	{
		StartAlpha = CreateFloatConstant(1.0f);
	}
}

void UParticleModuleColor::Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const
{
	const FVector RGB = Evaluate(StartColor, StartColorCache, Owner.EmitterTime);
	const float Alpha = Evaluate(StartAlpha, StartAlphaCache, Owner.EmitterTime);
	Particle.BaseColor = Particle.Color = FLinearColor(RGB.X, RGB.Y, RGB.Z, Alpha);
}