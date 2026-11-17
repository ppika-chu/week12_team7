#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionVector.h"

class UParticleModuleLocationBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleLocationBase, UParticleModule)

public:
	UParticleModuleLocationBase() = default;
	virtual ~UParticleModuleLocationBase() = default;
};

// 생성 위치 오프셋. 이미터 위치 기준이며 이미터 회전을 따른다.
class UParticleModuleLocation : public UParticleModuleLocationBase
{
	DECLARE_CLASS(UParticleModuleLocation, UParticleModuleLocationBase)

	REFLECT_START(ClassName)
		PROPERTY_DISTRIBUTION(StartLocation)
	REFLECT_END()

public:
	UParticleModuleLocation();
	virtual ~UParticleModuleLocation() = default;

	virtual void Spawn(const FSpawnContext& Context) override;

public:
	FRawDistributionVector StartLocation;
};
