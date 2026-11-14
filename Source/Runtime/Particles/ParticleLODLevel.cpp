#include "EnginePCH.h"
#include "ParticleModule.h"
#include "ParticleModuleTypeDataBase.h"
#include "ParticleLODLevel.h"

UParticleLODLevel::UParticleLODLevel()
	: Level(0)
	, bEnabled(true)
{
}

void UParticleLODLevel::UpdateModuleLists()
{
	SpawnModules.Reset();
	UpdateModules.Reset();

	UParticleModule* Module;
	int32 TypeDataModuleIndex = -1;

	for (int32 i = 0; i < Modules.Num(); i++)
	{
		Module = Modules[i];
		if (!Module)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.Add(Module);
		}

		if (Module->bUpdateModule)
		{
			UpdateModules.Add(Module);
		}

		if (Module->IsA(UParticleModuleTypeDataBase::StaticClass()))
		{
			if (!Module->bSpawnModule && !Module->bUpdateModule)
			{
				TypeDataModuleIndex = i;
			}
		}
	}

	//TypeData는 Modules에 들어올 수 있으므로 전용 포인터로 옮긴다
	if (TypeDataModuleIndex != -1)
	{
		TypeDataModule = static_cast<UParticleModuleTypeDataBase*>(Modules[TypeDataModuleIndex]);
		Modules.RemoveAt(TypeDataModuleIndex);
	}
}
