#include "EnginePCH.h"
#include "Asset/AssetManager.h"
#include "UObject/UObjectGlobals.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "ParticleModuleSpawn.h"
#include "ParticleModuleLifetime.h"
#include "ParticleSystem.h"

namespace
{
const FString DefaultParticleSystemKey = "DefaultParticleSystem";
} // namespace

UParticleSystem* UParticleSystem::GetDefault()
{
	if (UParticleSystem* Found = UAssetManager::GetAssetByPath<UParticleSystem>(DefaultParticleSystemKey))
	{
		return Found;
	}

	UParticleSystem* System = NewObject<UParticleSystem>();
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>(System);
	UParticleLODLevel* LOD = NewObject<UParticleLODLevel>(Emitter);

	LOD->RequiredModule = NewObject<UParticleModuleRequired>(LOD);
	LOD->SpawnModule = NewObject<UParticleModuleSpawn>(LOD);

	//기본 모듈
	LOD->Modules.Add(NewObject<UParticleModuleLifetime>(LOD));

	// TODO : 기본 모듈이 생기면 여기서 추가 (Lifetime, Location, Velocity, Color, Size)

	Emitter->LODLevels.Add(LOD);
	System->Emitters.Add(Emitter);

	System->UpdateAndBuild();
	UAssetManager::Get().RegisterAsset(DefaultParticleSystemKey, System);
	return System;
}

//에셋 구조가 정해지거나 바뀐 뒤 반드시 호출
void UParticleSystem::UpdateAndBuild()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		for (UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			LOD->UpdateModuleLists();
		}
	}

	BuildEmitters();
}

// 파티클 한 개의 크기 계산
void UParticleSystem::BuildEmitters()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		Emitter->Build();
	}
}
