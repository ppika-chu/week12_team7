#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"

class UParticleModuleLifetimeBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleLifetimeBase, UParticleModule)

public:
	UParticleModuleLifetimeBase() = default;
	virtual ~UParticleModuleLifetimeBase() = default;
};

// 파티클의 수명 (초)
class UParticleModuleLifetime : public UParticleModuleLifetimeBase
{
	DECLARE_CLASS(UParticleModuleLifetime, UParticleModuleLifetimeBase)

	REFLECT_START(ClassName)
		CLASS_TOOLTIP("How long each particle lives.")
		PROPERTY_DISTRIBUTION(Lifetime) PROPERTY_TOOLTIP("Particle lifetime (s).")
	REFLECT_END()

public:
	UParticleModuleLifetime();
	virtual ~UParticleModuleLifetime() = default;

	virtual void Spawn(const FSpawnContext& Context) override;

public:
	FRawDistributionFloat Lifetime;
};
