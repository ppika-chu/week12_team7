#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"

class UParticleLODLevel;

class UParticleModuleSubUV : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSubUV, UParticleModule)

	REFLECT_START(UParticleModuleSubUV)
		PROPERTY_DISTRIBUTION(SubImageIndex)
	REFLECT_END()

public:
	UParticleModuleSubUV();

	void InitializeDefaults() override;
	void FinalUpdate(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	static bool ValidateLOD(const UParticleLODLevel& LODLevel, FString& Error);

	FRawDistributionFloat SubImageIndex;
};
