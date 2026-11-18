#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionVector.h"

class UParticleModuleAcceleration : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleAcceleration, UParticleModule)

	REFLECT_START(UParticleModuleAcceleration)
		CLASS_TOOLTIP("Applies acceleration throughout each particle's life.")
		PROPERTY_DISTRIBUTION(Acceleration) PROPERTY_TOOLTIP("Acceleration (m/s^2) in simulation axes. Curve X = time in the loop (s).")
	REFLECT_END()

public:
	UParticleModuleAcceleration();
	void Spawn(const FSpawnContext& Context) override;
	void Update(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	FRawDistributionVector Acceleration;

private:
	struct FAccelerationPayload
	{
		FVector Value;
		uint32 bInitialized;
	};

	static_assert(sizeof(FAccelerationPayload) == 16);
	static_assert(alignof(FAccelerationPayload) == alignof(float));

	static bool HasPayload(const FParticleEmitterInstance& Owner, int32 Offset);
	void InitializePayload(FParticleEmitterInstance& Owner, FAccelerationPayload& Payload) const;
};
