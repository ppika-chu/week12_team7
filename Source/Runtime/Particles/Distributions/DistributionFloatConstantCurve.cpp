#include "EnginePCH.h"
#include "DistributionFloatConstantCurve.h"

float UDistributionFloatConstantCurve::GetValue(float Time, FRandomStream*) const
{
	return ConstantCurve.Eval(Time, 0.0f);
}

int32 UDistributionFloatConstantCurve::AddPoint(float InVal, float OutVal)
{
	const int32 Index = ConstantCurve.AddPoint(InVal, OutVal);
	if (Index >= 0)
	{
		MarkDirty();
	}
	return Index;
}

bool UDistributionFloatConstantCurve::RemovePoint(int32 Index)
{
	if (!ConstantCurve.RemovePoint(Index))
	{
		return false;
	}
	MarkDirty();
	return true;
}

void UDistributionFloatConstantCurve::Reset()
{
	ConstantCurve.Reset();
	MarkDirty();
}