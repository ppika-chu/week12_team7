#pragma once

#include <algorithm>
#include <cmath>
#include "Containers/Array.h"
#include "Math/Vector.h"

template<typename T>
struct FInterpCurvePoint
{
	float InVal = 0.0f;
	T OutVal = T{};
};

template<typename T>
class FInterpCurve
{
public:
	int32 AddPoint(float InVal, const T& OutVal)
	{
		if (!std::isfinite(InVal))
		{
			return -1;
		}

		const int32 Index = LowerBound(InVal);
		if (Index < Points.Num() && Points[Index].InVal == InVal)
		{
			// 같은 시간의 키는 값 교체
			Points[Index].OutVal = OutVal;
		}
		else
		{
			Points.Insert(FInterpCurvePoint<T>{ InVal, OutVal }, Index);
		}
		return Index;
	}

	bool RemovePoint(int32 Index)
	{
		if (Index < 0 || Index >= Points.Num())
		{
			return false;
		}
		Points.RemoveAt(Index);
		return true;
	}

	void Reset() { Points.Reset(); }

	const TArray<FInterpCurvePoint<T>>& GetPoints() const { return Points; }

	T Eval(float InVal, const T& DefaultValue = T{}) const
	{
		if (Points.IsEmpty() || std::isnan(InVal))
		{
			return DefaultValue;
		}

		const int32 RightIndex = LowerBound(InVal);
		if (RightIndex == 0)
		{
			return Points[0].OutVal;
		}
		if (RightIndex == Points.Num())
		{
			return Points[Points.Num() - 1].OutVal;
		}

		const FInterpCurvePoint<T>& Right = Points[RightIndex];
		if (InVal == Right.InVal)
		{
			return Right.OutVal;
		}

		// 선형 보간
		const FInterpCurvePoint<T>& Left = Points[RightIndex - 1];
		const double TimeRange = static_cast<double>(Right.InVal) - Left.InVal;
		const float Alpha = static_cast<float>((static_cast<double>(InVal) - Left.InVal) / TimeRange);
		return Left.OutVal * (1.0f - Alpha) + Right.OutVal * Alpha;
	}

private:
	int32 LowerBound(float InVal) const
	{
		const auto It = std::lower_bound(Points.begin(), Points.end(), InVal,
			[](const FInterpCurvePoint<T>& Point, float Time)
			{
				return Point.InVal < Time;
			});
		return static_cast<int32>(It - Points.begin());
	}

	TArray<FInterpCurvePoint<T>> Points;
};

using FInterpCurveFloat = FInterpCurve<float>;
using FInterpCurveVector = FInterpCurve<FVector>;