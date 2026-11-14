#include "EnginePCH.h"
#include "ParticleModuleLifetime.h"
#include "Particles/ParticleEmitterInstance.h"

void UParticleModuleLifetime::InitializeDefaults()
{
	if (!Lifetime)
	{
		Lifetime = CreateFloatConstant(1.0f);
	}
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance& Owner, FBaseParticle& Particle) const
{
	const float MaxLifetime = Evaluate(Lifetime, LifetimeCache, Owner.EmitterTime);
	// 0 이하 수명은 무한 생존
	Particle.OneOverMaxLifetime = MaxLifetime > 0.0f ? 1.0f / MaxLifetime : 0.0f;
}