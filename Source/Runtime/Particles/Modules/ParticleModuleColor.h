#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleColor.generated.h"

UCLASS()
class UParticleModuleColor : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionVector* StartColor = nullptr;

	UPROPERTY(EditAnywhere)
	UDistributionFloat* StartAlpha = nullptr;

	virtual void InitializeDefaults() override;
	virtual void Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const override;

private:
	mutable FRawDistributionVector StartColorCache;
	mutable FRawDistributionFloat StartAlphaCache;
};