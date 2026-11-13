#pragma once

#include "Math/EngineMath.h"

// 키 사이를 어떻게 이을지 (UE EInterpCurveMode의 일부)
enum class EInterpCurveMode : uint8
{
	Linear,    // 직선으로 잇는다.
	Constant,  // 다음 키까지 앞 키의 값을 유지한다 (계단).
	CurveAuto, // 부드러운 곡선. 접선을 AutoSetTangents가 이웃 키로 계산한다.
	CurveUser, // 부드러운 곡선. 접선을 직접 지정한다.
};

// 커브의 키 하나 (UE FInterpCurvePoint)
template<typename T>
struct FInterpCurvePoint
{
	float InVal = 0.0f;  // 입력 (파티클에서는 보통 RelativeTime, 0~1)
	T OutVal = T();      // 그 입력에서의 값
	T ArriveTangent = T(); // 이 키로 들어올 때의 기울기
	T LeaveTangent = T();  // 이 키에서 나갈 때의 기울기
	EInterpCurveMode InterpMode = EInterpCurveMode::Linear; // 이 키부터 다음 키까지의 보간 방식

	FInterpCurvePoint() = default;

	FInterpCurvePoint(float InInVal, const T& InOutVal, EInterpCurveMode InMode = EInterpCurveMode::Linear)
		: InVal(InInVal), OutVal(InOutVal), InterpMode(InMode)
	{
	}
};

// 키 목록으로 정의한 커브 (UE FInterpCurve).
// T는 float, FVector, FVector2처럼 덧셈과 float 곱셈이 되는 타입이면 된다.
template<typename T>
struct FInterpCurve
{
	TArray<FInterpCurvePoint<T>> Points;

	// 키를 InVal 순서를 지키며 추가하고, 들어간 위치를 돌려준다.
	int32 AddPoint(float InVal, const T& OutVal, EInterpCurveMode Mode = EInterpCurveMode::Linear)
	{
		int32 Index = 0;
		while (Index < Points.Num() && Points[Index].InVal < InVal)
		{
			++Index;
		}
		Points.Insert(FInterpCurvePoint<T>(InVal, OutVal, Mode), Index);
		return Index;
	}

	void Reset()
	{
		Points.Empty();
	}

	// InVal 위치의 값을 계산한다. 키가 없으면 Default를 돌려준다.
	// 첫 키보다 앞이면 첫 키 값, 마지막 키보다 뒤면 마지막 키 값을 쓴다.
	T Eval(float InVal, const T& Default = T()) const
	{
		const int32 NumPoints = Points.Num();
		if (NumPoints == 0)
		{
			return Default;
		}
		if (NumPoints == 1 || InVal <= Points[0].InVal)
		{
			return Points[0].OutVal;
		}
		if (InVal >= Points[NumPoints - 1].InVal)
		{
			return Points[NumPoints - 1].OutVal;
		}

		// InVal을 포함하는 구간 [Index, Index + 1]을 찾는다. 파티클 커브는 키가 몇 개뿐이라 순차 탐색으로 충분하다.
		int32 Index = 0;
		while (Index < NumPoints - 2 && InVal >= Points[Index + 1].InVal)
		{
			++Index;
		}

		const FInterpCurvePoint<T>& P0 = Points[Index];
		const FInterpCurvePoint<T>& P1 = Points[Index + 1];
		const float Diff = P1.InVal - P0.InVal;
		if (Diff <= 0.0f)
		{
			return P0.OutVal;
		}
		const float Alpha = (InVal - P0.InVal) / Diff;

		switch (P0.InterpMode)
		{
		case EInterpCurveMode::Constant:
			return P0.OutVal;
		case EInterpCurveMode::Linear:
			return FMath::Lerp(P0.OutVal, P1.OutVal, Alpha);
		default:
			// 접선은 "InVal 1당 변화량"이므로 구간 길이를 곱해 Alpha(0~1) 기준으로 바꾼다.
			return CubicInterp(P0.OutVal, P0.LeaveTangent * Diff, P1.OutVal, P1.ArriveTangent * Diff, Alpha);
		}
	}

	// CurveAuto 키의 접선을 이웃 키로부터 계산한다 (Catmull-Rom).
	// 키를 추가하거나 값을 바꾼 뒤 호출한다. 양 끝 키의 접선은 0이다.
	void AutoSetTangents()
	{
		const int32 NumPoints = Points.Num();
		for (int32 Index = 0; Index < NumPoints; ++Index)
		{
			FInterpCurvePoint<T>& Point = Points[Index];
			if (Point.InterpMode != EInterpCurveMode::CurveAuto)
			{
				continue;
			}

			T Tangent = T();
			if (Index > 0 && Index < NumPoints - 1)
			{
				const FInterpCurvePoint<T>& Prev = Points[Index - 1];
				const FInterpCurvePoint<T>& Next = Points[Index + 1];
				const float Span = Next.InVal - Prev.InVal;
				if (Span > 0.0f)
				{
					Tangent = (Next.OutVal - Prev.OutVal) * (1.0f / Span);
				}
			}
			Point.ArriveTangent = Tangent;
			Point.LeaveTangent = Tangent;
		}
	}

private:
	// 3차 에르미트 보간. 두 끝값과 두 끝 기울기로 곡선을 만든다.
	static T CubicInterp(const T& P0, const T& T0, const T& P1, const T& T1, float A)
	{
		const float A2 = A * A;
		const float A3 = A2 * A;
		return P0 * (2.0f * A3 - 3.0f * A2 + 1.0f)
			+ T0 * (A3 - 2.0f * A2 + A)
			+ P1 * (-2.0f * A3 + 3.0f * A2)
			+ T1 * (A3 - A2);
	}
};
