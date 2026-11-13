#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"

#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleTypeDataBase.h"

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODIndex) const
{
	if (LODIndex < 0 || LODIndex >= LODLevels.Num())
	{
		return nullptr;
	}
	return LODLevels[LODIndex];
}

void UParticleEmitter::UpdateModuleLists()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}
	Build();
}

void UParticleEmitter::Build()
{
	if (UParticleLODLevel* HighLODLevel = GetLODLevel(0))
	{
		// TypeData가 메모리 배치 계산 전에 필요한 모듈 포인터를 기억하게 한다.
		if (HighLODLevel->TypeDataModule)
		{
			HighLODLevel->TypeDataModule->CacheModuleInfo(this);
		}
	}
	CacheEmitterModuleInfo();
}

// UE UParticleEmitter::CacheEmitterModuleInfo의 핵심 부분.
// 파티클 칸 하나 = [ FBaseParticle | TypeData payload | 모듈 A payload | 모듈 B payload | ... ]
void UParticleEmitter::CacheEmitterModuleInfo()
{
	ModuleOffsetMap.Empty();
	ModuleInstanceOffsetMap.Empty();
	ModulesNeedingInstanceData.Empty();

	ParticleSize = sizeof(FBaseParticle);
	ReqInstanceBytes = 0;
	TypeDataOffset = 0;
	TypeDataInstanceOffset = -1;

	UParticleLODLevel* HighLODLevel = GetLODLevel(0);
	if (!HighLODLevel)
	{
		return;
	}

	// TypeData가 먼저 자리를 잡는다.
	UParticleModuleTypeDataBase* HighTypeData = HighLODLevel->TypeDataModule;
	if (HighTypeData)
	{
		const uint32 ReqBytes = HighTypeData->RequiredBytes(nullptr);
		if (ReqBytes > 0)
		{
			TypeDataOffset = ParticleSize;
			ParticleSize += static_cast<int32>(ReqBytes);
		}

		const uint32 InstanceBytes = HighTypeData->RequiredBytesPerInstance();
		if (InstanceBytes > 0)
		{
			TypeDataInstanceOffset = ReqInstanceBytes;
			ReqInstanceBytes += static_cast<int32>(InstanceBytes);
		}
	}

	// 나머지 모듈은 목록 순서대로 뒤에 붙는다.
	for (int32 ModuleIndex = 0; ModuleIndex < HighLODLevel->Modules.Num(); ++ModuleIndex)
	{
		UParticleModule* Module = HighLODLevel->Modules[ModuleIndex];
		if (!Module || Module->IsA<UParticleModuleTypeDataBase>())
		{
			continue;
		}

		// 파티클마다 붙는 payload
		const uint32 ReqBytes = Module->RequiredBytes(HighTypeData);
		if (ReqBytes > 0)
		{
			ModuleOffsetMap.Add(Module, static_cast<uint32>(ParticleSize));
			ParticleSize += static_cast<int32>(ReqBytes);
		}

		// 이미터 인스턴스마다 하나씩 붙는 데이터
		const uint32 InstanceBytes = Module->RequiredBytesPerInstance();
		if (InstanceBytes > 0)
		{
			ModuleInstanceOffsetMap.Add(Module, static_cast<uint32>(ReqInstanceBytes));
			ModulesNeedingInstanceData.Add(Module);

			// 다른 LOD의 같은 자리 모듈도 같은 위치를 쓰게 등록해 둔다.
			// 그러면 실행 중에 최상위 LOD 모듈을 다시 찾지 않아도 된다.
			for (int32 LODIndex = 1; LODIndex < LODLevels.Num(); ++LODIndex)
			{
				UParticleLODLevel* LODLevel = LODLevels[LODIndex];
				if (LODLevel && ModuleIndex < LODLevel->Modules.Num() && LODLevel->Modules[ModuleIndex])
				{
					ModuleInstanceOffsetMap.Add(LODLevel->Modules[ModuleIndex], static_cast<uint32>(ReqInstanceBytes));
				}
			}

			ReqInstanceBytes += static_cast<int32>(InstanceBytes);
		}
	}
}
