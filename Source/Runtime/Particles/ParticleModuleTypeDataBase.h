#pragma once

#include "ParticleModules.h"
#include "ParticleModuleTypeDataBase.generated.h"

UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
	GENERATED_BODY()

public:
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override { return 0; }
};