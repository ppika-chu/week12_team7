#pragma once

#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "ParticleModuleSpawn.generated.h"

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UDistributionFloat* Rate = nullptr;

	virtual void InitializeDefaults() override;
	float GetRate(FParticleEmitterInstance& Owner) const;

private:
	mutable FRawDistributionFloat RateCache;
};