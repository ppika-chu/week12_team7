#pragma once

#include "DistributionFloat.h"
#include "DistributionVector.h"

// 원본을 소유하지 않는 평가 캐시
struct FRawDistribution
{
protected:
	bool NeedsUpdate(const UDistribution& Source) const
	{
		return CachedRevision != Source.GetRevision();
	}

	void UpdateCacheState(const UDistribution& Source) const
	{
		CachedRevision = Source.GetRevision();
		bConstant = Source.IsConstant();
	}

	void Invalidate()
	{
		CachedRevision = 0;
		bConstant = false;
	}

	mutable bool bConstant = false;

private:
	mutable uint64 CachedRevision = 0;
};

struct FRawDistributionFloat : public FRawDistribution
{
	void SetDistribution(UDistributionFloat* InDistribution)
	{
		Distribution = InDistribution;
		Invalidate();
	}

	UDistributionFloat* GetDistribution() const { return Distribution; }
	float GetValue(float Time, FRandomStream* RandomStream = nullptr) const;

private:
	UDistributionFloat* Distribution = nullptr;
	mutable float CachedValue = 0.0f;
};

struct FRawDistributionVector : public FRawDistribution
{
	void SetDistribution(UDistributionVector* InDistribution)
	{
		Distribution = InDistribution;
		Invalidate();
	}

	UDistributionVector* GetDistribution() const { return Distribution; }
	FVector GetValue(float Time, FRandomStream* RandomStream = nullptr) const;

private:
	UDistributionVector* Distribution = nullptr;
	mutable FVector CachedValue = FVector::ZeroVector;
};