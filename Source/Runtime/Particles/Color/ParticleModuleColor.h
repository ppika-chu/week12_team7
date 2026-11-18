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
		CLASS_TOOLTIP("Initial color and opacity.")
		PROPERTY_DISTRIBUTION(StartColor) PROPERTY_TOOLTIP("Initial color (RGB, 0 to 1).")
		PROPERTY_DISTRIBUTION(StartAlpha) PROPERTY_TOOLTIP("Initial opacity (0 to 1).")
		PROPERTY(bClampAlpha) PROPERTY_TOOLTIP("Clamp alpha to 0 to 1.")
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
		CLASS_TOOLTIP("Color and opacity over life.")
		PROPERTY_DISTRIBUTION(ColorOverLife) PROPERTY_TOOLTIP("Color over life (RGB). Curve X = life (0 to 1).")
		PROPERTY_DISTRIBUTION(AlphaOverLife) PROPERTY_TOOLTIP("Opacity over life. Usually 1 to 0 to fade out.")
		PROPERTY(bClampAlpha) PROPERTY_TOOLTIP("Clamp alpha to 0 to 1.")
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
