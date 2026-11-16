#include "EnginePCH.h"
#include "Distributions/DistributionFloat.h"

float UDistributionFloatConstant::GetValue(float F, FRandomStream* RandomStream) const
{
	return Constant;
}

float UDistributionFloatUniform::GetValue(float F, FRandomStream* RandomStream) const
{
	return Min + (Max - Min) * GetRandomValue(RandomStream);
}

float UDistributionFloatConstantCurve::GetValue(float F, FRandomStream* RandomStream) const
{
	return ConstantCurve.Eval(F, 0.0f);
}

float UDistributionFloatUniformCurve::GetValue(float F, FRandomStream* RandomStream) const
{
	const FVector2 Range = ConstantCurve.Eval(F, FVector2(0.0f, 0.0f));
	return Range.X + (Range.Y - Range.X) * GetRandomValue(RandomStream);
}
