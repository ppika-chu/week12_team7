#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleColorOverLife.generated.h"

UCLASS()
class UParticleModuleColorOverLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionVector* ColorOverLife = nullptr;

	UPROPERTY(EditAnywhere)
	UDistributionFloat* AlphaOverLife = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Update(FParticleEmitterInstance& Owner, FBaseParticle& Particle, float DeltaTime) const override;

private:
	mutable FRawDistributionVector ColorOverLifeCache;
	mutable FRawDistributionFloat AlphaOverLifeCache;
};