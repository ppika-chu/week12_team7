#include "EnginePCH.h"
#include "ParticleModuleTypeDataBeam.h"
#include "ParticleEmitterInstances.h"

FParticleEmitterInstance* UParticleModuleTypeDataBeam::CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate)
{
	FParticleEmitterInstance* Instance = new FParticleBeamEmitterInstance(InComponent);
	Instance->InitParameters(InTemplate);
	Instance->Init();
	return Instance;
}
