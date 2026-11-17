#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionVector.h"

class UParticleModuleSizeBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSizeBase, UParticleModule)

public:
	UParticleModuleSizeBase() = default;
	virtual ~UParticleModuleSizeBase() = default;
};

// 생성 시 크기 (Initial Size)
class UParticleModuleSize : public UParticleModuleSizeBase
{
	DECLARE_CLASS(UParticleModuleSize, UParticleModuleSizeBase)

	REFLECT_START(ClassName)
		PROPERTY_DISTRIBUTION(StartSize)
	REFLECT_END()

public:
	UParticleModuleSize();
	virtual ~UParticleModuleSize() = default;

	virtual void Spawn(const FSpawnContext& Context) override;

public:
	FRawDistributionVector StartSize;
};

// 수명에 따른 크기 배율 (Size By Life). F = 파티클의 RelativeTime
class UParticleModuleSizeMultiplyLife : public UParticleModuleSizeBase
{
	DECLARE_CLASS(UParticleModuleSizeMultiplyLife, UParticleModuleSizeBase)

	REFLECT_START(ClassName)
		PROPERTY_DISTRIBUTION(LifeMultiplier)
		PROPERTY(MultiplyX)
		PROPERTY(MultiplyY)
		PROPERTY(MultiplyZ)
	REFLECT_END()

public:
	UParticleModuleSizeMultiplyLife();
	virtual ~UParticleModuleSizeMultiplyLife() = default;

	virtual void Spawn(const FSpawnContext& Context) override;
	virtual void Update(const FUpdateContext& Context) override;

public:
	FRawDistributionVector LifeMultiplier;
	bool MultiplyX = true;
	bool MultiplyY = true;
	bool MultiplyZ = true;
};

// 속도에 따른 크기 배율 (Size Scale By Speed). 빠를수록 길어지는 스파크 등에 쓴다.
class UParticleModuleSizeScaleBySpeed : public UParticleModuleSizeBase
{
	DECLARE_CLASS(UParticleModuleSizeScaleBySpeed, UParticleModuleSizeBase)

	REFLECT_START(ClassName)
		PROPERTY(SpeedScale)
		PROPERTY(MaxScale)
	REFLECT_END()

public:
	UParticleModuleSizeScaleBySpeed();
	virtual ~UParticleModuleSizeScaleBySpeed() = default;

	virtual void Update(const FUpdateContext& Context) override;

public:
	// UE는 FVector2D지만 리플렉션 때문에 FVector를 쓴다. X, Y만 사용한다.
	FVector SpeedScale = FVector(1.0f, 1.0f, 0.0f);
	FVector MaxScale = FVector(1.0f, 1.0f, 0.0f);
};
