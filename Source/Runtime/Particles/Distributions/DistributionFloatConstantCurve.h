#pragma once

#include "DistributionFloat.h"
#include "Math/InterpCurve.h"
#include "DistributionFloatConstantCurve.generated.h"

UCLASS()
class UDistributionFloatConstantCurve : public UDistributionFloat
{
	GENERATED_BODY()

public:
	virtual float GetValue(float Time, FRandomStream* RandomStream = nullptr) const override;
	int32 AddPoint(float InVal, float OutVal);
	bool RemovePoint(int32 Index);
	void Reset();

	const FInterpCurveFloat& GetCurve() const { return ConstantCurve; }

private:
	FInterpCurveFloat ConstantCurve;
};