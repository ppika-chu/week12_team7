#include "EnginePCH.h"
#include "Particles/ParticleModuleSpawn.h"

UParticleModuleSpawn::UParticleModuleSpawn()
{
	UDistributionFloatConstant* RateDistribution = CreateDefaultSubobject<UDistributionFloatConstant>("RateDistribution");
	RateDistribution->SetConstant(20.0f);
	Rate.Distribution = RateDistribution;

	UDistributionFloatConstant* RateScaleDistribution = CreateDefaultSubobject<UDistributionFloatConstant>("RateScaleDistribution");
	RateScaleDistribution->SetConstant(1.0f);
	RateScale.Distribution = RateScaleDistribution;

	UDistributionFloatConstant* BurstScaleDistribution = CreateDefaultSubobject<UDistributionFloatConstant>("BurstScaleDistribution");
	BurstScaleDistribution->SetConstant(1.0f);
	BurstScale.Distribution = BurstScaleDistribution;
}
