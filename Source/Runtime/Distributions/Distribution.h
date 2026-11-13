#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Math/RandomStream.h"
#include "Math/Noise.h"

// 모든 Distribution의 부모 (UE Distributions/Distribution.h).
// Distribution = "값을 어떻게 정할지"를 담은 객체. 상수, 범위 랜덤, 시간에 따른 커브 등이 있다.
// 모듈은 값을 직접 갖지 않고 Distribution을 가져서, 같은 모듈로 여러 방식의 값을 쓸 수 있다.
class UDistribution : public UObject
{
	DECLARE_CLASS(UDistribution, UObject)

public:
	UDistribution() = default;
	virtual ~UDistribution() = default;

protected:
	// 랜덤 Distribution이 쓰는 [0, 1) 난수. 스트림이 있으면 스트림에서, 없으면 전역 난수에서 뽑는다.
	// (UE DIST_GET_RANDOM_VALUE)
	static float GetRandomValue(FRandomStream* RandomStream)
	{
		return RandomStream ? RandomStream->GetFraction() : FMath::FRand();
	}
};
