#pragma once

#include "DistributionFloat.h"
#include "DistributionFloatConstant.generated.h"

UCLASS()
class UDistributionFloatConstant : public UDistributionFloat
{
	GENERATED_BODY()

public:
	virtual bool IsConstant() const override { return true; }
	virtual float GetValue(float, FRandomStream* = nullptr) const override { return Constant; }

	float GetConstant() const { return Constant; }

	void SetConstant(float InConstant)
	{
		Constant = InConstant;
		MarkDirty();
	}

private:
	UPROPERTY(EditAnywhere)
	float Constant = 0.0f;
};