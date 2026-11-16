#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSystemComponent.h"
#include "ParticleSystemActor.generated.h"

// 새 파티클 시스템의 월드 배치용 액터
UCLASS()
class AParticleSystemActor : public AActor
{
	GENERATED_BODY()

public:
	AParticleSystemActor();

private:
	UParticleSystemComponent* ParticleComponent = nullptr;
};