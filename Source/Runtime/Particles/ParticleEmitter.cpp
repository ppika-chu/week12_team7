#include "EnginePCH.h"
#include "ParticleHelper.h"
#include "ParticleLODLevel.h"
#include "ParticleEmitterInstances.h"
#include "ParticleModules.h"
#include "ParticleModuleTypeDataBase.h"
#include "ParticleEmitter.h"

//이미터 한 개의 크기 계산
void UParticleEmitter::Build()
{
	const int32 LODCount = LODLevels.Num();
	if (LODCount > 0)
	{
		//ParticleMoudleTypeDataBase에 대한 Build 코드가 있었음...
		//TODO : TypeBase에 대한 Build가 필요한가?
		
		// Cache particle size/offset data for all LOD Levels
		CacheEmitterModuleInfo();
	}
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ModuleOffsetMap.Empty();
	TypeDataOffset = 0;

	//BaseParticle부터 시작
	ParticleSize = sizeof(FBaseParticle);

	UParticleLODLevel* HighLODLevel = LODLevels[0];
	assert(HighLODLevel);

	UParticleModuleTypeDataBase* HighTypeData = HighLODLevel->TypeDataModule;
	if (HighTypeData)
	{
		int32 ReqBytes = HighTypeData->RequiredBytes(static_cast<UParticleModuleTypeDataBase*>(nullptr));
		if (ReqBytes)
		{
			TypeDataOffset = ParticleSize;
			ParticleSize += ReqBytes;
		}

		//TODO : PerInstanceDataBlock?
		//int32 TempInstanceBytes = HighTypeData->RequiredBytesPerInstance();
		//if (TempInstanceBytes)
		//{
		//	TypeDataInstanceOffset = ReqInstanceBytes;
		//	ReqInstanceBytes += TempInstanceBytes;
		//}
	}

	//TODO : mesh rotation active if alignment is set
	//bMeshRotationActive = (RequiredModule->ScreenAlignment == PSA_Velocity || RequiredModule->ScreenAlignment == PSA_AwayFromCenter);

	UParticleLODLevel* HighLOD = LODLevels[0];

	for (int32 i = 0; i < HighLOD->Modules.Num(); ++i)
	{
		UParticleModule* Module = HighLOD->Modules[i];
		int32 ReqBytes = Module->RequiredBytes(HighTypeData);
		if (ReqBytes)
		{
			ModuleOffsetMap.Add(Module, ParticleSize);

			// 낮은 LOD도 모듈 순서가 같기 때문에 오프셋도 동일하게 설정한다.
			for (int32 LOD = 1; LOD < LODLevels.Num(); ++LOD)
			{
				ModuleOffsetMap.Add(LODLevels[LOD]->Modules[i], ParticleSize);
			}

			ParticleSize += ReqBytes;
		}
	}
}
