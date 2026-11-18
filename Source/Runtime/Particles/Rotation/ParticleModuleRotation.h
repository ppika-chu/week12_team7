#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"

class UParticleModuleRotation : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleRotation, UParticleModule)

	REFLECT_START(ClassName)
		CLASS_TOOLTIP("Initial rotation and spin for Sprite and Mesh particles.")
		PROPERTY_DISTRIBUTION(StartRotation) PROPERTY_TOOLTIP("Initial angle (degrees). Curve X = emitter time (s).")
		PROPERTY_DISTRIBUTION(StartRotationRate) PROPERTY_TOOLTIP("Spin speed (degrees/s). Curve X = emitter time (s).")
	REFLECT_END()

public:
	UParticleModuleRotation();
	void Spawn(const FSpawnContext& Context) override;

	FRawDistributionFloat StartRotation;
	FRawDistributionFloat StartRotationRate;
};
