#include "EnginePCH.h"
#include "ParticleSystemActor.h"

AParticleSystemActor::AParticleSystemActor()
{
	ParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>("UParticleSystemComponent");
	SetRootComponent(ParticleComponent);
}