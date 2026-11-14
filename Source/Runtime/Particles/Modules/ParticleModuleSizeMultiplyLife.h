#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleSizeMultiplyLife.generated.h"

UCLASS()
class UParticleModuleSizeMultiplyLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionVector* LifeMultiplier = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Update(FParticleEmitterInstance& Owner, FBaseParticle& Particle, float DeltaTime) const override;

private:
	mutable FRawDistributionVector LifeMultiplierCache;
};