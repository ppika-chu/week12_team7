#include "EnginePCH.h"
#include "RawDistribution.h"

float FRawDistributionFloat::GetValue(float Time, FRandomStream* RandomStream) const
{
	if (!Distribution)
	{
		return 0.0f;
	}

	if (NeedsUpdate(*Distribution))
	{
		UpdateCacheState(*Distribution);
		if (bConstant)
		{
			CachedValue = Distribution->GetValue(0.0f, nullptr);
		}
	}
	return bConstant ? CachedValue : Distribution->GetValue(Time, RandomStream);
}

FVector FRawDistributionVector::GetValue(float Time, FRandomStream* RandomStream) const
{
	if (!Distribution)
	{
		return FVector::ZeroVector;
	}

	if (NeedsUpdate(*Distribution))
	{
		UpdateCacheState(*Distribution);
		if (bConstant)
		{
			CachedValue = Distribution->GetValue(0.0f, nullptr);
		}
	}
	return bConstant ? CachedValue : Distribution->GetValue(Time, RandomStream);
}