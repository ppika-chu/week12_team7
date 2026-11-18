#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"

class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
	DECLARE_CLASS(UParticleModuleTypeDataRibbon, UParticleModuleTypeDataBase)

	REFLECT_START(UParticleModuleTypeDataRibbon)
		CLASS_TOOLTIP("Connect particles in spawn order into a ribbon trail.")
		PROPERTY(Width) PROPERTY_TOOLTIP("Ribbon width (m).")
	REFLECT_END()

public:
	float Width = 0.1f;

	FParticleEmitterInstance* CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate) override;

	// 리본도 빔 셰이더로 그린다. 빔과 같은 머티리얼로 바꾼다
	void SetToSensibleDefaults(UParticleEmitter* Owner) override;
};
