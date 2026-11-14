#include "EnginePCH.h"
#include "GameFramework/Actor/Emitter.h"

#include "Asset/AssetManager.h"

AEmitter::AEmitter()
{
	ParticleSystemComponent = CreateDefaultSubobject<UParticleSystemComponent>("ParticleSystemComponent");
	SetRootComponent(ParticleSystemComponent);
	ParticleSystemComponent->SetTemplate(UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem"));
}
