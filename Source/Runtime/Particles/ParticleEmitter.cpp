#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/Color/ParticleModuleColor.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#include "Distributions/DistributionVector.h"

void UParticleSpriteEmitter::SetToSensibleDefaults()
{
	UParticleLODLevel* LODLevel = GetLODLevel(0);
	if (!LODLevel)
	{
		return;
	}
	// Spawn Rate 20, Lifetime 1초는 모듈 생성자 기본값과 같다
	LODLevel->AddModule<UParticleModuleLifetime>();

	// UE 25cm
	UParticleModuleSize* SizeModule = LODLevel->AddModule<UParticleModuleSize>();
	if (UDistributionVectorUniform* SizeDist = Cast<UDistributionVectorUniform>(SizeModule->StartSize.Distribution))
	{
		SizeDist->Min = FVector(0.25f, 0.25f, 0.25f);
		SizeDist->Max = FVector(0.25f, 0.25f, 0.25f);
	}

	// UE (-10, -10, 50) ~ (10, 10, 100) cm/s
	UParticleModuleVelocity* VelModule = LODLevel->AddModule<UParticleModuleVelocity>();
	if (UDistributionVectorUniform* VelDist = Cast<UDistributionVectorUniform>(VelModule->StartVelocity.Distribution))
	{
		VelDist->Min = FVector(-0.1f, -0.1f, 0.5f);
		VelDist->Max = FVector(0.1f, 0.1f, 1.0f);
	}

	// UE: 색은 흰색 유지, 알파 1 → 0. 생성자 기본은 흰색 → 검정이라 끝 키만 흰색으로 바꾼다
	UParticleModuleColorOverLife* ColorModule = LODLevel->AddModule<UParticleModuleColorOverLife>();
	if (UDistributionVectorConstantCurve* ColorDist = Cast<UDistributionVectorConstantCurve>(ColorModule->ColorOverLife.Distribution))
	{
		ColorDist->AddPoint(1.0f, FVector(1.0f, 1.0f, 1.0f));
	}
}

FParticleEmitterInstance* UParticleSpriteEmitter::CreateInstance(IParticleEmitterInstanceOwner& InComponent)
{
	if (LODLevels.Num() == 0 || !LODLevels[0] || !LODLevels[0]->RequiredModule)
	{
		return nullptr;
	}

	// 아직 메모리 배치를 계산하지 않았다면 먼저 계산한다.
	if (ParticleSize == 0)
	{
		UpdateModuleLists();
	}

	UParticleModuleTypeDataBase* TypeData = LODLevels[0]->TypeDataModule;

	// Mesh일 경우 메시 생성
	if (TypeData)
	{
		return TypeData->CreateInstance(InComponent, this);
	}

	FParticleEmitterInstance* Instance = new FParticleSpriteEmitterInstance(InComponent);
	Instance->InitParameters(this);
	Instance->Init();
	return Instance;
}

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

void UParticleEmitter::PostEditChangeProperty(const FProperty& Property)
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->PeakActiveParticles = 1;
		}
	}
	UpdateModuleLists();
	Super::PostEditChangeProperty(Property);
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
			for (int32 LODIndex = 1; LODIndex < LODLevels.Num(); ++LODIndex)
			{
				UParticleLODLevel* LOD = LODLevels[LODIndex];
				if (LOD && ModuleIndex < LOD->Modules.Num() && LOD->Modules[ModuleIndex])
				{
					ModuleOffsetMap.Add(LOD->Modules[ModuleIndex], static_cast<uint32>(ParticleSize));
				}
			}
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

UParticleLODLevel* UParticleEmitter::AddLODLevel()
{
	UParticleLODLevel* LOD = NewObject<UParticleLODLevel>(this);
	LOD->Level = LODLevels.Num();
	LOD->RequiredModule = NewObject<UParticleModuleRequired>(LOD);
	LOD->SpawnModule = NewObject<UParticleModuleSpawn>(LOD);
	LODLevels.Add(LOD);
	return LOD;
}
