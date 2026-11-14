#include "EnginePCH.h"
#include "ParticleModuleColorOverLife.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleColorOverLife::InitializeDefaults()
{
	if (!ColorOverLife)
	{
		ColorOverLife = CreateVectorConstant(FVector(1.0f));
	}
	if (!AlphaOverLife)
	{
		AlphaOverLife = CreateFloatConstant(1.0f);
	}
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance&, FBaseParticle& Particle, float) const
{
	const FVector RGB = Evaluate(ColorOverLife, ColorOverLifeCache, Particle.RelativeTime);
	const float Alpha = Evaluate(AlphaOverLife, AlphaOverLifeCache, Particle.RelativeTime);
	Particle.Color = FLinearColor(RGB.X, RGB.Y, RGB.Z, Alpha);
}