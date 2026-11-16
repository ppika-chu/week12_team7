#pragma once

#include "Distributions/Distribution.h"
#include "Math/InterpCurve.h"
#include "Math/Vector2.h"

// float 값을 내는 Distribution의 부모 (UE DistributionFloat.h).
// F는 "어디서의 값인가"를 뜻하는 입력이다. 파티클 모듈에서는 보통 파티클의 RelativeTime(0~1)을 넘긴다.
// 상수와 범위 랜덤은 F를 무시하고, 커브는 F 위치의 값을 돌려준다.
class UDistributionFloat : public UDistribution
{
	DECLARE_CLASS(UDistributionFloat, UDistribution)

public:
	UDistributionFloat() = default;
	virtual ~UDistributionFloat() = default;

	virtual float GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const { return 0.0f; }
};

// 항상 같은 값 (UE DistributionFloatConstant)
class UDistributionFloatConstant : public UDistributionFloat
{
	DECLARE_CLASS(UDistributionFloatConstant, UDistributionFloat)

	REFLECT_START(ClassName)
		PROPERTY(Constant)
	REFLECT_END()

public:
	UDistributionFloatConstant() = default;
	virtual ~UDistributionFloatConstant() = default;

	virtual float GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;
	virtual bool IsConstant() const override { return true; }
	float GetConstant() const { return Constant; }
	void SetConstant(float Value) { Constant = Value; MarkDirty(); }

private:
	float Constant = 0.0f;
};

// [Min, Max] 사이의 랜덤 값. 호출할 때마다 새로 뽑는다 (UE DistributionFloatUniform).
// 파티클 모듈의 Spawn에서 부르면 파티클마다 다른 값이 된다. 예: 수명 0.5~1초.
class UDistributionFloatUniform : public UDistributionFloat
{
	DECLARE_CLASS(UDistributionFloatUniform, UDistributionFloat)

	REFLECT_START(ClassName)
		PROPERTY(Min)
		PROPERTY(Max)
	REFLECT_END()

public:
	UDistributionFloatUniform() = default;
	virtual ~UDistributionFloatUniform() = default;

	virtual float GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;

public:
	float Min = 0.0f;
	float Max = 0.0f;
};

// F에 따라 변하는 값 (UE DistributionFloatConstantCurve).
// 예: 수명에 따라 크기 배율 1 → 2.
class UDistributionFloatConstantCurve : public UDistributionFloat
{
	DECLARE_CLASS(UDistributionFloatConstantCurve, UDistributionFloat)

public:
	UDistributionFloatConstantCurve() = default;
	virtual ~UDistributionFloatConstantCurve() = default;

	virtual float GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;
	int32 AddPoint(float InVal, float OutVal, EInterpCurveMode Mode = EInterpCurveMode::Linear)
	{
		const int32 Index = ConstantCurve.AddPoint(InVal, OutVal, Mode);
		if (Index >= 0) { MarkDirty(); }
		return Index;
	}
	bool RemovePoint(int32 Index)
	{
		if (!ConstantCurve.RemovePoint(Index)) { return false; }
		MarkDirty();
		return true;
	}
	void Reset() { ConstantCurve.Reset(); MarkDirty(); }
	const FInterpCurveFloat& GetCurve() const { return ConstantCurve; }

public:
	FInterpCurve<float> ConstantCurve;
};

// F 위치에서 [최소 커브, 최대 커브] 사이의 랜덤 값 (UE DistributionFloatUniformCurve).
// 키 값의 X가 최소, Y가 최대다.
class UDistributionFloatUniformCurve : public UDistributionFloat
{
	DECLARE_CLASS(UDistributionFloatUniformCurve, UDistributionFloat)

public:
	UDistributionFloatUniformCurve() = default;
	virtual ~UDistributionFloatUniformCurve() = default;

	virtual float GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const override;

public:
	FInterpCurve<FVector2> ConstantCurve;
};

// 모듈이 멤버로 갖는 Distribution 포장 (UE FRawDistributionFloat).
// 모듈 코드는 어떤 종류의 Distribution인지 몰라도 GetValue 하나로 값을 얻는다.
// 상수는 변경 이력에 따라 캐시하고, 랜덤과 커브는 호출할 때마다 평가한다.
struct FRawDistributionFloat : public FRawDistribution
{
	UDistributionFloat* Distribution = nullptr;

	bool IsCreated() const { return Distribution != nullptr; }
	void SetDistribution(UDistributionFloat* Value) { Distribution = Value; Invalidate(); }
	UDistributionFloat* GetDistribution() const { return Distribution; }

	float GetValue(float F = 0.0f, FRandomStream* RandomStream = nullptr) const
	{
		if (!Distribution) { return 0.0f; }
		if (NeedsUpdate(*Distribution))
		{
			UpdateCacheState(*Distribution);
			if (bConstant) { CachedValue = Distribution->GetValue(0.0f, nullptr); }
		}
		return bConstant ? CachedValue : Distribution->GetValue(F, RandomStream);
	}

private:
	mutable float CachedValue = 0.0f;
};
