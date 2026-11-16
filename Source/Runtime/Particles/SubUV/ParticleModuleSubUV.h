#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"
#include "ParticleModuleSubUV.generated.h"

class UParticleLODLevel;

UCLASS()
class UParticleModuleSubUV : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSubUV();

	void InitializeDefaults() override;
	void FinalUpdate(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	static bool ValidateLOD(const UParticleLODLevel& LODLevel, FString& Error);

	FRawDistributionFloat SubImageIndex;
};