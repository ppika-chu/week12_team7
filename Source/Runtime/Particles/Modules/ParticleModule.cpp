#include "EnginePCH.h"
#include "ParticleModule.h"
#include "Particles/Distributions/RawDistribution.h"
#include "Particles/Distributions/DistributionFloatConstant.h"
#include "Particles/Distributions/DistributionVectorConstant.h"

UDistributionFloat* UParticleModule::CreateFloatConstant(float Value)
{
	auto* Distribution = NewObject<UDistributionFloatConstant>(this);
	Distribution->SetConstant(Value);
	return Distribution;
}

UDistributionVector* UParticleModule::CreateVectorConstant(const FVector& Value)
{
	auto* Distribution = NewObject<UDistributionVectorConstant>(this);
	Distribution->SetConstant(Value);
	return Distribution;
}

float UParticleModule::Evaluate(UDistributionFloat* Source, FRawDistributionFloat& Cache, float Time) const
{
	if (Cache.GetDistribution() != Source)
	{
		Cache.SetDistribution(Source);
	}
	return Cache.GetValue(Time);
}

FVector UParticleModule::Evaluate(UDistributionVector* Source, FRawDistributionVector& Cache, float Time) const
{
	if (Cache.GetDistribution() != Source)
	{
		Cache.SetDistribution(Source);
	}
	return Cache.GetValue(Time);
}