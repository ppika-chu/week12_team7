#include "EnginePCH.h"
#include "ParticleModuleTypeDataBeam.h"
#include "ParticleEmitterInstances.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Render/Material.h"

void UParticleModuleTypeDataBeam::SetToSensibleDefaults(UParticleEmitter* Owner)
{
	UseBeamMaterial(Owner);
}

void UParticleModuleTypeDataBeam::UseBeamMaterial(UParticleEmitter* Owner)
{
	UMaterial* BeamMaterial = UAssetManager::GetAssetByPath<UMaterial>("ParticleBeamMaterial");
	if (!Owner || !BeamMaterial)
	{
		return;
	}
	for (UParticleLODLevel* LODLevel : Owner->LODLevels)
	{
		UParticleModuleRequired* Required = LODLevel ? LODLevel->RequiredModule : nullptr;
		// 렌더러와 같은 기준: 같은 빔 셰이더면 텍스처만 바꾼 복사본도 그대로 둔다
		if (Required && (!Required->Material || Required->Material->Shader != BeamMaterial->Shader))
		{
			Required->Material = BeamMaterial;
		}
	}
}

FParticleEmitterInstance* UParticleModuleTypeDataBeam::CreateInstance(IParticleEmitterInstanceOwner& InComponent, UParticleEmitter* InTemplate)
{
	FParticleEmitterInstance* Instance = new FParticleBeamEmitterInstance(InComponent);
	Instance->InitParameters(InTemplate);
	Instance->Init();
	return Instance;
}
