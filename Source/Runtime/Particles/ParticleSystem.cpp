#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"

#include "Particles/ParticleEmitter.h"

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
