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

void DestroyParticleObject(UObject* Root)
{
	if (!Root) return;
	TArray<UObject*> OwnedObjects;
	OwnedObjects.Add(Root);
	for (int32 ParentIndex = 0; ParentIndex < OwnedObjects.Num(); ++ParentIndex)
	{
		for (UObject* Object : GUObjectArray)
		{
			// 무관한 객체의 Outer는 이미 삭제됐을 수 있으므로 역참조하지 않는다.
			if (Object->GetOuter() == OwnedObjects[ParentIndex])
			{
				OwnedObjects.Add(Object);
			}
		}
	}
	// 자식부터 지운다
	for (int32 Index = OwnedObjects.Num() - 1; Index >= 0; --Index) delete OwnedObjects[Index];
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
