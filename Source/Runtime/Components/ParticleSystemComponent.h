#pragma once

#include "Components/PrimitiveComponent.h"
#include "Math/RandomStream.h"
#include "Particles/ParticleEmitterInstanceOwner.h"
#include "Particles/ParticleSystem.h"

struct FParticleEmitterInstance;
struct FParticleDynamicData;

// 파티클 시스템 에셋(Template)을 월드에서 실행하는 컴포넌트 (UE ParticleSystemComponent.h)
// Template의 이미터마다 인스턴스를 하나씩 만들어 소유하고, 매 프레임 Tick한다.
class UParticleSystemComponent : public UPrimitiveComponent, public IParticleEmitterInstanceOwner
{
	DECLARE_CLASS(UParticleSystemComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Template)
		PROPERTY(bAutoActivate)
		PROPERTY(EmitterDelay)
		PROPERTY(RandomSeed)
	REFLECT_END()

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent() override;

	virtual void BeginPlay() override;
	virtual void EndPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;
	virtual FBox CalcLocalBounds() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetTemplate(UParticleSystem* NewTemplate);
	UParticleSystem* GetTemplate() const { return Template; }

	// 게임 코드가 부르는 시작/정지 (UE와 같이 UActorComponent의 가상 함수를 재정의)
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;
	virtual bool ShouldActivate() const override;

	// Activate / Deactivate가 부르는 실제 처리
	// bFlagAsJustAttached: true면 첫 Tick에 이전 위치를 현재 위치로 맞춘다 (bJustRegistered).
	void ActivateSystem(bool bFlagAsJustAttached = false);
	void DeactivateSystem();
	void ResetParticles(bool bEmptyInstances = false);
	void KillParticlesForced();

	bool HasCompleted() const;
	int32 GetNumActiveParticles() const;
	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }

	// 이번 프레임 시뮬레이션 결과의 스냅샷을 새로 만든다. SceneProxy가 받아 가지고 삭제한다.
	FParticleDynamicData* CreateDynamicData();

	// IParticleEmitterInstanceOwner
	virtual FVector GetComponentLocation() const override;
	virtual FMatrix GetComponentToWorld() const override;
	virtual FRandomStream& GetRandomStream() override { return RandomStream; }
	virtual float GetEmitterDelay() const override { return EmitterDelay; }
	virtual bool GetJustRegistered() const override { return bJustRegistered; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

private:
	void InitializeSystem();
	void InitParticles();
	// 모든 이미터의 파티클 범위(월드)를 10% 넓힌 것. 파티클이 없으면 false
	bool CalcParticleWorldBounds(FBox& OutBounds) const;
	FBox CalcWorldBounds() const;
	// UE FinalizeTickComponent의 바운드 갱신. 범위를 벗어났거나 5초가 지나면 프록시의 바운드를 다시 계산하게 한다.
	void UpdateBoundsIfNeeded(float DeltaTime);

public:
	UParticleSystem* Template = nullptr;
	// BeginPlay에서 자동으로 시작할지
	bool bAutoActivate = true;
	// 모든 이미터에 추가로 거는 지연 (초)
	float EmitterDelay = 0.0f;
	// 0이면 시작할 때마다 다른 시드
	int32 RandomSeed = 0;
	// 바운드가 커지지 않아도 이 시간마다 다시 계산해 줄인다 (초)
	float MaxTimeBeforeForceUpdateTransform = 5.0f;

private:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	FRandomStream RandomStream;

	// 프록시에 마지막으로 알린 바운드 (월드). 파티클이 이 밖으로 나가면 다시 알린다.
	FBox Bounds;
	bool bHasBounds = false;
	float TimeSinceLastForceUpdateTransform = 0.0f;

	// 등록된 뒤 아직 Tick하지 않음 (UE와 같은 이름)
	bool bJustRegistered = false;
	bool bWasDeactivated = false;
	bool bWasCompleted = false;
	// 한 번이라도 활성화됐는지. 게임에서 다시 켤 때 전부 초기화할지 판단한다.
	bool bHasBeenActivated = false;
};
