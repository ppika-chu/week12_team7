#pragma once

#include "Distribution.h"
#include "DistributionFloat.generated.h"

// Float 값의 공통 평가
UCLASS()
class UDistributionFloat : public UDistribution
{
	GENERATED_BODY()

public:
	virtual bool IsConstant() const override { return false; }
	virtual float GetValue(float Time, FRandomStream* RandomStream = nullptr) const = 0;
};