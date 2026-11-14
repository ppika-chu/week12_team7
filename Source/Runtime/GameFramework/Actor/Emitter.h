#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSystemComponent.h"

// 파티클 시스템 하나를 월드에 배치하는 액터 (UE Particles/Emitter.h)
class AEmitter : public AActor
{
	DECLARE_CLASS(AEmitter, AActor)

public:
	AEmitter();
	virtual ~AEmitter() = default;

	UParticleSystemComponent* GetParticleSystemComponent() const { return ParticleSystemComponent; }

private:
	UParticleSystemComponent* ParticleSystemComponent = nullptr;
};
