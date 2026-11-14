#pragma once

#include "UObject/Object.h"
#include "ParticleModule.generated.h"

struct FBaseParticle;
struct FParticleEmitterInstance;
struct FVector;
struct FRawDistributionFloat;
struct FRawDistributionVector;
class UDistributionFloat;
class UDistributionVector;

UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	virtual void InitializeDefaults() {}
	virtual void Spawn(FParticleEmitterInstance&, FBaseParticle&) const {}
	virtual void Update(FParticleEmitterInstance&, FBaseParticle&, float) const {}

protected:
	UDistributionFloat* CreateFloatConstant(float Value);
	UDistributionVector* CreateVectorConstant(const FVector& Value);
	float Evaluate(UDistributionFloat* Source, FRawDistributionFloat& Cache, float Time) const;
	FVector Evaluate(UDistributionVector* Source, FRawDistributionVector& Cache, float Time) const;
};