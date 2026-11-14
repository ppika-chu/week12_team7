#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleLocation.generated.h"

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionVector* StartLocation = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const override;

private:
	mutable FRawDistributionVector StartLocationCache;
};