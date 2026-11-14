#pragma once

#include "Distribution.h"
#include "Math/Vector.h"
#include "DistributionVector.generated.h"

// Vector 값의 공통 평가
UCLASS()
class UDistributionVector : public UDistribution
{
	GENERATED_BODY()

public:
	virtual bool IsConstant() const override { return false; }
	virtual FVector GetValue(float Time, FRandomStream* RandomStream = nullptr) const = 0;
};