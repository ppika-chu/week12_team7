#pragma once

#include "ParticleModule.h"
#include "Render/Material.h"
#include "ParticleModuleRequired.generated.h"

// LOD별 머티리얼 설정
UCLASS()
class UParticleModuleRequired : public UParticleModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	UMaterial* Material = nullptr;
};