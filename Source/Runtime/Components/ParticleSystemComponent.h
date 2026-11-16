#pragma once

#include "PrimitiveComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleReplayData.h"
#include "ParticleSystemComponent.generated.h"

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	UParticleSystemComponent();

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }
	void ResetParticles();
	int32 GetNumActiveParticles() const;

	void TickComponent(float DeltaTime) override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	FBox CalcLocalBounds() const override { return LocalBounds; }
	bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT) override;
	void AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const override;
	void PostEditChangeProperty(const FProperty& Property) override;
	void OnTransformDirty() override;

protected:
	void OnRegister() override;
	void OnUnregister() override;

private:
	TArray<FDynamicSpriteEmitterReplayData> BuildReplayData();
	void SendRenderDynamicData();

	// 공유 설정과 컴포넌트별 실행 상태
	UPROPERTY(VisibleAnywhere)
	UParticleSystem* Template = nullptr;
	UPROPERTY(EditAnywhere)
	int32 MaxParticles = 256;

	TArray<TUniquePtr<FParticleEmitterInstance>> EmitterInstances;
	FBox LocalBounds{ FVector::ZeroVector, FVector::ZeroVector };
};