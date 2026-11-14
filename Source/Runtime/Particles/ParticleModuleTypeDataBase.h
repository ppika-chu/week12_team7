#pragma once

#include "ParticleModule.h"
#include "ParticleModuleTypeDataBase.generated.h"

//이미터가 어떤 종류인가를 정하는 모듈
//Sprite, Mesh
UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataBase();
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override { return 0; }
};