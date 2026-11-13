#pragma once

#include "Distributions/Distribution.h"
#include "Math/InterpCurve.h"
#include "Math/Vector.h"

// FVector 값을 내는 Distribution의 부모 (UE DistributionVector.h).
// 위치, 속도, 크기, 색(RGB)처럼 세 값이 한 묶음인 설정에 쓴다.
class UDistributionVector : public UDistribution
{
	DECLARE_CLASS(UDistributionVector, UDistribution)

public:
	UDistributionVector() = default;
	virtual ~UDistributionVector() = default;

	virtual FVector GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const { return FVector(); }
};

// 항상 같은 벡터 (UE DistributionVectorConstant)
class UDistributionVectorConstant : public UDistributionVector
{
	DECLARE_CLASS(UDistributionVectorConstant, UDistributionVector)

	REFLECT_START(ClassName)
		PROPERTY(Constant)
	REFLECT_END()

public:
	UDistributionVectorConstant() = default;
	virtual ~UDistributionVectorConstant() = default;

	virtual FVector GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;

public:
	FVector Constant;
};

// 축마다 [Min, Max] 사이에서 따로 뽑은 랜덤 벡터 (UE DistributionVectorUniform).
// 예: 속도 (-10, -10, 50) ~ (10, 10, 100) → 위로 날아가며 옆으로 조금 퍼진다.
// UE의 축 고정(LockedAxes), 대칭(MirrorFlags) 옵션은 넣지 않았다.
class UDistributionVectorUniform : public UDistributionVector
{
	DECLARE_CLASS(UDistributionVectorUniform, UDistributionVector)

	REFLECT_START(ClassName)
		PROPERTY(Min)
		PROPERTY(Max)
	REFLECT_END()

public:
	UDistributionVectorUniform() = default;
	virtual ~UDistributionVectorUniform() = default;

	virtual FVector GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;

public:
	FVector Min;
	FVector Max;
};

// F에 따라 변하는 벡터 (UE DistributionVectorConstantCurve).
// 예: 수명에 따라 색 노랑 → 빨강.
class UDistributionVectorConstantCurve : public UDistributionVector
{
	DECLARE_CLASS(UDistributionVectorConstantCurve, UDistributionVector)

public:
	UDistributionVectorConstantCurve() = default;
	virtual ~UDistributionVectorConstantCurve() = default;

	virtual FVector GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;

public:
	FInterpCurve<FVector> ConstantCurve;
};

// F 위치에서 [최소 커브, 최대 커브] 사이의 랜덤 벡터 (UE DistributionVectorUniformCurve).
// UE는 최소·최대를 한 키에 묶지만(FTwoVectors), 여기서는 커브 두 개로 나눴다. 두 커브의 키 위치는 달라도 된다.
class UDistributionVectorUniformCurve : public UDistributionVector
{
	DECLARE_CLASS(UDistributionVectorUniformCurve, UDistributionVector)

public:
	UDistributionVectorUniformCurve() = default;
	virtual ~UDistributionVectorUniformCurve() = default;

	virtual FVector GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;

public:
	FInterpCurve<FVector> MinCurve;
	FInterpCurve<FVector> MaxCurve;
};

// 모듈이 멤버로 갖는 Distribution 포장 (UE FRawDistributionVector)
struct FRawDistributionVector
{
	UDistributionVector* Distribution = nullptr;

	bool IsCreated() const { return Distribution != nullptr; }

	FVector GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const
	{
		return Distribution ? Distribution->GetValue(F, RandomStream) : FVector();
	}
};
