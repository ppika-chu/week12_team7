#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleVelocity.generated.h"

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionVector* StartVelocity = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const override;

private:
	mutable FRawDistributionVector StartVelocityCache;
};