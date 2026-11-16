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

public:
	UParticleModuleLifetime();
	virtual ~UParticleModuleLifetime() = default;

	virtual void Spawn(const FSpawnContext& Context) override;

public:
	FRawDistributionFloat Lifetime;
};
