#include "EnginePCH.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"

// 시스템에 속하는 새 이미터의 생성과 등록
UParticleEmitter* UParticleSystem::AddEmitter()
{
	UParticleEmitter* NewEmitter = NewObject<UParticleEmitter>(this);
	Emitters.Add(NewEmitter);

	return NewEmitter;
}