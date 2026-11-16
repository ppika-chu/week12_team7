#include "EnginePCH.h"
#include "Particles/ParticleLODLevel.h"

#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleTypeDataBase.h"

// UE UParticleLODLevel::UpdateModuleLists와 같은 규칙으로 목록을 나눈다.
void UParticleLODLevel::UpdateModuleLists()
{
	SpawnModules.Empty();
	UpdateModules.Empty();

	int32 TypeDataModuleIndex = -1;

	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		UParticleModule* Module = Modules[Index];
		if (!Module)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.Add(Module);
		}
		if (Module->bUpdateModule || Module->bFinalUpdateModule)
		{
			UpdateModules.Add(Module);
		}

		if (UParticleModuleTypeDataBase* TypeData = Cast<UParticleModuleTypeDataBase>(Module))
		{
			TypeDataModule = TypeData;
			// Spawn/Update에 참여하지 않는 TypeData는 일반 모듈 목록에서 빼고 TypeDataModule로만 보관한다.
			if (!Module->bSpawnModule && !Module->bUpdateModule)
			{
				TypeDataModuleIndex = Index;
			}
		}
	}

	if (TypeDataModuleIndex != -1)
	{
		Modules.RemoveAt(TypeDataModuleIndex);
	}
}
