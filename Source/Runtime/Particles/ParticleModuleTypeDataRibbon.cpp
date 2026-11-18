#include "EnginePCH.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"

#include "Particles/ParticleEmitterInstances.h"

FParticleEmitterInstance* UParticleModuleTypeDataRibbon::CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate)
{
	auto* Instance = new FParticleRibbonEmitterInstance(InComponent);
	Instance->InitParameters(InTemplate);
	Instance->Init();
	return Instance;
}
