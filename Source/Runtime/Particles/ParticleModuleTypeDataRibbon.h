#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"

class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataRibbon, UParticleModuleTypeDataBase)

	REFLECT_START(UParticleModuleTypeDataRibbon)
		PROPERTY(Width)
	REFLECT_END()

public:
	float Width = 0.1f;

	FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;
};
