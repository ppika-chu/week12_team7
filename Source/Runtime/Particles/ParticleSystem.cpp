#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Asset/AssetManager.h"

UParticleSystem* UParticleSystem::GetDefault()
{
	if (!UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem"))
	{
		UAssetManager::Get().CreateDefaultParticleSystem();
	}
	return UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem");
}

// 시스템에 속하는 새 스프라이트 이미터의 생성과 등록
UParticleEmitter* UParticleSystem::AddEmitter()
{
	UParticleEmitter* Emitter = NewObject<UParticleSpriteEmitter>(this);
	Emitters.Add(Emitter);
	return Emitter;
}

void UParticleSystem::UpdateAllModuleLists()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			Emitter->UpdateModuleLists();
		}
	}
}

void UParticleSystem::BuildEmitters()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			Emitter->Build();
		}
	}
}
