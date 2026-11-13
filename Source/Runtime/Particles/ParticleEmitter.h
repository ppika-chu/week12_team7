#pragma once

#include "UObject/Object.h"
#include "Containers/Array.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;

// 하나의 파티클 방출 설정
UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	// 새 LOD 설정의 생성과 연결
	UParticleLODLevel* AddLODLevel();

	const TArray<UParticleLODLevel*>& GetLODLevels() const
	{
		return LODLevels;
	}

private:
	// 배열 순서에 따른 LOD 설정 목록
	TArray<UParticleLODLevel*> LODLevels;
};