#include "EnginePCH.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleModuleTypeDataBeam.h"

void UParticleModuleTypeDataRibbon::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UParticleModuleTypeDataBeam::UseBeamMaterial(Owner);
}

FParticleEmitterInstance* UParticleModuleTypeDataRibbon::CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate)
{
	auto* Instance = new FParticleRibbonEmitterInstance(InComponent);
	Instance->InitParameters(InTemplate);
	Instance->Init();
	return Instance;
}
