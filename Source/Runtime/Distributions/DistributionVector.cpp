#include "EnginePCH.h"
#include "Distributions/DistributionVector.h"

FVector UDistributionVectorConstant::GetValue(float F, FRandomStream* RandomStream) const
{
	return Constant;
}

FVector UDistributionVectorUniform::GetValue(float F, FRandomStream* RandomStream) const
{
	// 축마다 난수를 따로 뽑는다. 한 번만 뽑아 세 축에 같이 쓰면 결과가 Min~Max 대각선 위에만 놓인다.
	const float RandX = GetRandomValue(RandomStream);
	const float RandY = GetRandomValue(RandomStream);
	const float RandZ = GetRandomValue(RandomStream);
	return FVector(
		Min.X + (Max.X - Min.X) * RandX,
		Min.Y + (Max.Y - Min.Y) * RandY,
		Min.Z + (Max.Z - Min.Z) * RandZ);
}

FVector UDistributionVectorConstantCurve::GetValue(float F, FRandomStream* RandomStream) const
{
	return ConstantCurve.Eval(F, FVector());
}

FVector UDistributionVectorUniformCurve::GetValue(float F, FRandomStream* RandomStream) const
{
	const FVector LocalMin = MinCurve.Eval(F, FVector());
	const FVector LocalMax = MaxCurve.Eval(F, FVector());
	const float RandX = GetRandomValue(RandomStream);
	const float RandY = GetRandomValue(RandomStream);
	const float RandZ = GetRandomValue(RandomStream);
	return FVector(
		LocalMin.X + (LocalMax.X - LocalMin.X) * RandX,
		LocalMin.Y + (LocalMax.Y - LocalMin.Y) * RandY,
		LocalMin.Z + (LocalMax.Z - LocalMin.Z) * RandZ);
}
