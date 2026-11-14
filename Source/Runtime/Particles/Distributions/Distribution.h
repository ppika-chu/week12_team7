#pragma once

#include "UObject/Object.h"
#include "Distribution.generated.h"

struct FRandomStream;

// 분포 변경 이력과 캐시 갱신
UCLASS()
class UDistribution : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsConstant() const = 0;

	uint64 GetRevision() const { return Revision; }

	virtual void PostEditChangeProperty(const FProperty&) override
	{
		MarkDirty();
	}

	using Super::Serialize;
	virtual void Serialize(json& Handle, bool bIsLoading) override;
	virtual void Serialize(FStructuredArchive::FRecord Record) override;

protected:
	void MarkDirty() { ++Revision; }

private:
	uint64 Revision = 1;	// 설정값 변경 이력
};