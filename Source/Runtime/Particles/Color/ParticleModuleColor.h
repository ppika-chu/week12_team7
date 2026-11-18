#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"

class UParticleModuleColorBase : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleColorBase, UParticleModule)

public:
	UParticleModuleColorBase() = default;
	virtual ~UParticleModuleColorBase() = default;
};

// 생성 시 색 (Initial Color)
class UParticleModuleColor : public UParticleModuleColorBase
{
	DECLARE_CLASS(UParticleModuleColor, UParticleModuleColorBase)

	REFLECT_START(ClassName)
		PROPERTY_DISTRIBUTION(StartColor)
		PROPERTY_DISTRIBUTION(StartAlpha)
		PROPERTY(bClampAlpha)
	REFLECT_END()

public:
	UParticleModuleColor();
	virtual ~UParticleModuleColor() = default;

	virtual void Spawn(const FSpawnContext& Context) override;

public:
	FRawDistributionVector StartColor; // RGB
	FRawDistributionFloat StartAlpha;
	bool bClampAlpha = true;
};

// 수명에 따라 바뀌는 색 (Color Over Life). F = 파티클의 RelativeTime
class UParticleModuleColorOverLife : public UParticleModuleColorBase
{
	DECLARE_CLASS(UParticleModuleColorOverLife, UParticleModuleColorBase)

	REFLECT_START(ClassName)
		PROPERTY_DISTRIBUTION(ColorOverLife)
		PROPERTY_DISTRIBUTION(AlphaOverLife)
		PROPERTY(bClampAlpha)
	REFLECT_END()

public:
	UParticleModuleColorOverLife();
	virtual ~UParticleModuleColorOverLife() = default;

	virtual void Spawn(const FSpawnContext& Context) override;
	virtual void Update(const FUpdateContext& Context) override;

public:
	FRawDistributionVector ColorOverLife; // RGB
	FRawDistributionFloat AlphaOverLife;
	bool bClampAlpha = true;
};
