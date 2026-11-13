#pragma once

#include "UObject/Object.h"
#include "ParticleLODLevel.generated.h"

// 단일 LOD의 파티클 설정
UCLASS()
class UParticleLODLevel : public UObject
{
	GENERATED_BODY()

public:
	// 이미터 내부의 LOD 순번
	UPROPERTY(VisibleAnywhere)
	int32 Level = 0;

	// 해당 LOD의 사용 여부
	UPROPERTY(EditAnywhere)
	bool bEnabled = true;
};