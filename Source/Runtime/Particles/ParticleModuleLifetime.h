#pragma once

#include "ParticleModule.h"
#include "ParticleModuleLifetime.generated.h"

//파티클이 태어날 때 얼마나 살아있을지 정하는 모듈
UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLifetime();

	// 수명(초)의 범위. 파티클마다 이 사이에서 랜덤으로 뽑는다.
	UPROPERTY(EditAnywhere)
	float LifetimeMin = 1.f;

	UPROPERTY(EditAnywhere)
	float LifetimeMax = 1.f;

	virtual void Spawn(const FSpawnContext& Context) override;
};