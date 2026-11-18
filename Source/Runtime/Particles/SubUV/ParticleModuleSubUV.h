#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"

class UParticleLODLevel;

class UParticleModuleSubUV : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSubUV, UParticleModule)

	REFLECT_START(UParticleModuleSubUV)
		CLASS_TOOLTIP("Flipbook animation from an atlas texture. Set SubImages in Required.")
		PROPERTY_DISTRIBUTION(SubImageIndex) PROPERTY_TOOLTIP("Atlas cell to show (from 0). Use a curve over life to animate.")
	REFLECT_END()

public:
	UParticleModuleSubUV();

	void InitializeDefaults() override;
	void FinalUpdate(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	static bool ValidateLOD(const UParticleLODLevel& LODLevel, FString& Error);

	FRawDistributionFloat SubImageIndex;
};
