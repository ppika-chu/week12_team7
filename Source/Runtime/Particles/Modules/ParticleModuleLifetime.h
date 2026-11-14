#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleLifetime.generated.h"

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionFloat* Lifetime = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const override;

private:
	mutable FRawDistributionFloat LifetimeCache;
};