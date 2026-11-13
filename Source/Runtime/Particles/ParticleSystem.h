#pragma once

#include "Asset/RenderAsset.h"
#include "Containers/Array.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

// 여러 이미터를 묶는 파티클 시스템 에셋
UCLASS()
class UParticleSystem : public URenderAsset
{
	GENERATED_BODY()

public:
	// 새 이미터 설정의 생성과 연결
	UParticleEmitter* AddEmitter();

	// 이미터 설정 목록의 읽기 전용 접근
	const TArray<UParticleEmitter*>& GetEmitters() const
	{
		return Emitters;
	}

private:
	// 시스템을 구성하는 이미터 설정 목록
	TArray<UParticleEmitter*> Emitters;
};