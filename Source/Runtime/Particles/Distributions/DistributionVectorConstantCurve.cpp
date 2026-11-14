#include "EnginePCH.h"
#include "DistributionVectorConstantCurve.h"

FVector UDistributionVectorConstantCurve::GetValue(float Time, FRandomStream*) const
{
	return ConstantCurve.Eval(Time, FVector::ZeroVector);
}

int32 UDistributionVectorConstantCurve::AddPoint(float InVal, const FVector& OutVal)
{
	const int32 Index = ConstantCurve.AddPoint(InVal, OutVal);
	if (Index >= 0)
	{
		MarkDirty();
	}
	return Index;
}

bool UDistributionVectorConstantCurve::RemovePoint(int32 Index)
{
	if (!ConstantCurve.RemovePoint(Index))
	{
		return false;
	}
	MarkDirty();
	return true;
}

void UDistributionVectorConstantCurve::Reset()
{
	ConstantCurve.Reset();
	MarkDirty();
}