#include "EnginePCH.h"
#include "ParticleModuleTypeDataMesh.h"
#include "ParticleEmitterInstances.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate)
{
	FParticleEmitterInstance* Instance = new FParticleMeshEmitterInstance(InComponent);
	Instance->InitParameters(InTemplate);
	Instance->Init();
	return Instance;
}
