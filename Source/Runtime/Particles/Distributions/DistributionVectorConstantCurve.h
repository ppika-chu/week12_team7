#pragma once

#include "DistributionVector.h"
#include "Math/InterpCurve.h"
#include "DistributionVectorConstantCurve.generated.h"

UCLASS()
class UDistributionVectorConstantCurve : public UDistributionVector
{
	GENERATED_BODY()

public:
	virtual FVector GetValue(float Time, FRandomStream* RandomStream = nullptr) const override;
	int32 AddPoint(float InVal, const FVector& OutVal);
	bool RemovePoint(int32 Index);
	void Reset();

	const FInterpCurveVector& GetCurve() const { return ConstantCurve; }

private:
	FInterpCurveVector ConstantCurve;
};