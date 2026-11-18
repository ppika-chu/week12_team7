#include "EnginePCH.h"
#include "ParticleModuleTypeDataMesh.h"
#include "ParticleEmitterInstances.h"
#include "Asset/AssetManager.h"

void UParticleModuleTypeDataMesh::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	if (!Mesh)
	{
		Mesh = UAssetManager::GetAssetByPath<UStaticMesh>("Cube");
	}
}

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate)
{
	FParticleEmitterInstance* Instance = new FParticleMeshEmitterInstance(InComponent);
	Instance->InitParameters(InTemplate);
	Instance->Init();
	return Instance;
}
