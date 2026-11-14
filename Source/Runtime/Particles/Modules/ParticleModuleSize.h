#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleSize.generated.h"

UCLASS()
class UParticleModuleSize : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionVector* StartSize = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const override;

private:
	mutable FRawDistributionVector StartSizeCache;
};