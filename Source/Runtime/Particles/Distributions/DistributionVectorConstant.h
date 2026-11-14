#pragma once

#include "DistributionVector.h"
#include "DistributionVectorConstant.generated.h"

UCLASS()
class UDistributionVectorConstant : public UDistributionVector
{
	GENERATED_BODY()

public:
	virtual bool IsConstant() const override { return true; }
	virtual FVector GetValue(float, FRandomStream* = nullptr) const override { return Constant; }

	FVector GetConstant() const { return Constant; }

	void SetConstant(const FVector& InConstant)
	{
		Constant = InConstant;
		MarkDirty();
	}

private:
	UPROPERTY(EditAnywhere)
	FVector Constant = FVector::ZeroVector;
};