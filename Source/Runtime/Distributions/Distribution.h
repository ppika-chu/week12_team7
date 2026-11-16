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
	virtual bool IsConstant() const { return false; }
	uint64 GetRevision() const { return Revision; }
	virtual void PostEditChangeProperty(const FProperty& Property) override
	{
		Super::PostEditChangeProperty(Property);
		MarkDirty();
	}
	using Super::Serialize;
	virtual void Serialize(json& Handle, bool bIsLoading) override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

protected:
	void MarkDirty() { ++Revision; }
	// 랜덤 Distribution이 쓰는 [0, 1) 난수. 스트림이 있으면 스트림에서, 없으면 전역 난수에서 뽑는다.
	// (UE DIST_GET_RANDOM_VALUE)
	static float GetRandomValue(FRandomStream* RandomStream)
	{
		return RandomStream ? RandomStream->GetFraction() : FMath::FRand();
	}

private:
	uint64 Revision = 1;
};

// Minsu's evaluation cache is shared by the richer CY raw distributions.
struct FRawDistribution
{
protected:
	bool NeedsUpdate(const UDistribution& Source) const
	{
		return CachedDistribution != &Source || CachedRevision != Source.GetRevision();
	}
	void UpdateCacheState(const UDistribution& Source) const
	{
		CachedDistribution = &Source;
		CachedRevision = Source.GetRevision();
		bConstant = Source.IsConstant();
	}
	void Invalidate()
	{
		CachedDistribution = nullptr;
		CachedRevision = 0;
		bConstant = false;
	}
	mutable bool bConstant = false;

private:
	mutable const UDistribution* CachedDistribution = nullptr;
	mutable uint64 CachedRevision = 0;
};
