#pragma once

#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include <cmath>
#include <memory>
#include <stdexcept>

inline void Require(bool Condition, const char* Message)
{
	if (!Condition) { throw std::runtime_error(Message); }
}

inline bool NearlyEqual(float A, float B)
{
	return std::abs(A - B) < 0.001f;
}

inline void SetParticleTestFloat(FRawDistributionFloat& Target, float Value)
{
	auto* Constant = NewObject<UDistributionFloatConstant>();
	Constant->SetConstant(Value);
	Target.SetDistribution(Constant);
}

inline UParticleSystem* MakeParticleTestSystem(float Rate = 10.0f, float Lifetime = 10.0f, bool bLocalSpace = false)
{
	auto* System = NewObject<UParticleSystem>();
	UParticleLODLevel* LOD = System->AddEmitter()->AddLODLevel();
	LOD->RequiredModule->bUseLocalSpace = bLocalSpace;
	SetParticleTestFloat(LOD->SpawnModule->Rate, Rate);
	SetParticleTestFloat(LOD->AddModule<UParticleModuleLifetime>()->Lifetime, Lifetime);
	System->UpdateAllModuleLists();
	return System;
}

// Lifecycle tests use the actual component without creating a window or a scene proxy.
class UTestParticleSystemComponent : public UParticleSystemComponent
{
	DECLARE_CLASS(UTestParticleSystemComponent, UParticleSystemComponent)

protected:
	bool ShouldCreateRenderState() const override { return false; }
};

struct FParticleTestFixture
{
	std::unique_ptr<UWorld> World{ NewObject<UWorld>() };
	std::unique_ptr<UTestParticleSystemComponent> Component{ NewObject<UTestParticleSystemComponent>() };

	explicit FParticleTestFixture(UParticleSystem* System, bool bAutoActivate = true)
	{
		World->SetWorldType(EWorldType::EditorPreview);
		Component->Template = System;
		Component->bAutoActivate = bAutoActivate;
		Component->RandomSeed = 12345;
		Component->RegisterComponentWithWorld(World.get());
	}
	~FParticleTestFixture() { Component->UnregisterComponent(); }
	void Tick(float DeltaTime) { World->GetTickTaskManager().RunAllTickGroups(DeltaTime); }
};
