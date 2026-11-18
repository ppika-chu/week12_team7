#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"

class UParticleModuleVelocityBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleVelocityBase, UParticleModule)

	REFLECT_START(ClassName)
		PROPERTY(bInWorldSpace) PROPERTY_TOOLTIP("On: velocity in world axes. Off: follows emitter rotation.")
	REFLECT_END()

public:
	UParticleModuleVelocityBase() = default;
	virtual ~UParticleModuleVelocityBase() = default;

public:
	// true면 속도를 이미터 회전과 상관없이 월드 축 기준으로 본다.
	bool bInWorldSpace = false;
};

// 초기 속도. StartVelocity(방향 지정) + StartVelocityRadial(이미터 중심에서 바깥쪽)
class UParticleModuleVelocity : public UParticleModuleVelocityBase
{
	DECLARE_CLASS(UParticleModuleVelocity, UParticleModuleVelocityBase)

	REFLECT_START(ClassName)
		CLASS_TOOLTIP("Initial speed and direction.")
		PROPERTY_DISTRIBUTION(StartVelocity) PROPERTY_TOOLTIP("Initial velocity (m/s). Curve X = time in the loop (s).")
		PROPERTY_DISTRIBUTION(StartVelocityRadial) PROPERTY_TOOLTIP("Extra speed outward from the emitter center (m/s).")
	REFLECT_END()

public:
	UParticleModuleVelocity();
	virtual ~UParticleModuleVelocity() = default;

	virtual void Spawn(const FSpawnContext& Context) override;

public:
	FRawDistributionVector StartVelocity;
	FRawDistributionFloat StartVelocityRadial;
};
