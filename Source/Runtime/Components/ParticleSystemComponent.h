#pragma once

#include "Components/PrimitiveComponent.h"
#include "Math/RandomStream.h"
#include "Particles/ParticleEmitterInstanceOwner.h"
#include "Particles/ParticleSystem.h"
#include "Core/TDelegate.h"
#include "Particles/ParticleHelper.h"

struct FParticleEmitterInstance;
struct FParticleDynamicData;

// 파티클 시스템 에셋의 이미터별 실행 상태를 소유한다.
class UParticleSystemComponent : public UPrimitiveComponent, public IParticleEmitterInstanceOwner
{
	DECLARE_CLASS(UParticleSystemComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Template)
		PROPERTY(bAutoActivate)
		PROPERTY(EmitterDelay)
		PROPERTY(RandomSeed)
		PROPERTY(MaxParticles)
		PROPERTY(TranslucencySortPriority)
	REFLECT_END()

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent() override;

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime) override;
	void PostEditChangeProperty(const FProperty& Property) override;
	FBox CalcLocalBounds() const override;
	FPrimitiveSceneProxy* CreateSceneProxy() override;
	bool LineTraceComponentLocal(const FRay& LocalRay, float& OutT) override;
	void AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const override;
	void OnTransformDirty() override;

	void SetTemplate(UParticleSystem* NewTemplate);
	UParticleSystem* GetTemplate() const { return Template; }
	void Activate(bool bReset = false) override;
	void Deactivate() override;
	bool ShouldActivate() const override;
	void ActivateSystem(bool bFlagAsJustAttached = false);
	void DeactivateSystem();
	void ResetParticles(bool bEmptyInstances = false);
	void KillParticlesForced();
	bool HasCompleted() const;
	int32 GetNumActiveParticles() const;
	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }
	FParticleDynamicData* CreateDynamicData();

	FVector GetComponentLocation() const override;
	FMatrix GetComponentToWorld() const override;
	FRandomStream& GetRandomStream() override { return RandomStream; }
	float GetEmitterDelay() const override { return EmitterDelay; }
	bool GetJustRegistered() const override { return bJustRegistered; }
	int32 GetMaxParticles() const override { return MaxParticles; }

	bool ParticleLineCheck(const FVector& Start, const FVector& End, FHitResult& OutHit) const override;
	void QueueParticleCollision(const FParticleCollisionEvent& Event) override;

	// Tick 종료 후 충돌 이벤트 전달
	DECLARE_DELEGATE(OnParticleCollision, const TArray<FParticleCollisionEvent>&)

	int32 GetTranslucencySortPriority() const { return TranslucencySortPriority; }
	void SetTranslucencySortPriority(int32 NewPriority);

protected:
	void OnRegister() override;
	void OnUnregister() override;

private:
	void InitializeSystem();
	void InitParticles();
	bool CalcParticleWorldBounds(FBox& OutBounds) const;
	FBox CalcWorldBounds() const;
	void UpdateBoundsIfNeeded(float DeltaTime);

public:
	UParticleSystem* Template = nullptr;
	bool bAutoActivate = true;
	float EmitterDelay = 0.0f;
	// 0이면 시작할 때마다 다른 시드
	int32 RandomSeed = 0;
	int32 MaxParticles = 256;
	float MaxTimeBeforeForceUpdateTransform = 5.0f;

private:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	FRandomStream RandomStream;
	FBox Bounds{ FVector::ZeroVector, FVector::ZeroVector };
	bool bHasBounds = false;
	float TimeSinceLastForceUpdateTransform = 0.0f;
	bool bJustRegistered = false;
	bool bWasDeactivated = false;
	bool bWasCompleted = false;
	bool bHasBeenActivated = false;
	int32 TranslucencySortPriority = 0;	// 반투명 정렬 우선순위
	TArray<FParticleCollisionEvent> PendingCollisionEvents; // 이번 Tick의 충돌 이벤트
};
