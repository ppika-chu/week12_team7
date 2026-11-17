#pragma once

#include "Particles/ParticleModule.h"

class UParticleEmitter;
class FParticleEmitterInstance;
class IParticleEmitterInstanceOwner;

// 이미터가 그리는 방식을 바꾸는 모듈
// TypeData가 없으면 Sprite 이미터
// LOD 레벨의 Modules 목록에 넣으면 UpdateModuleLists가 꺼내서 TypeDataModule로 옮긴다.
class UParticleModuleTypeDataBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleTypeDataBase, UParticleModule)

public:
	UParticleModuleTypeDataBase() = default;
	virtual ~UParticleModuleTypeDataBase() = default;

	// 이미터가 메모리 배치를 계산하기 전에 필요한 모듈 포인터를 기억해 둔다.
	virtual void CacheModuleInfo(UParticleEmitter* Emitter) {}
	// CreateInstance() 가 LOD 0의 TypeData가 있으면 생성을 위임
	virtual FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) 
	{
		return nullptr;
	}
};
