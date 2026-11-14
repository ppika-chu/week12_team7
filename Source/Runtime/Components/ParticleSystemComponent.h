#pragma once

#include "Components/PrimitiveComponent.h"
#include "Math/RandomStream.h"
#include "Particles/ParticleEmitterInstanceOwner.h"
#include "Particles/ParticleSystem.h"

struct FParticleEmitterInstance;
class FLineBatcher;

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

	// 렌더 전 확인용: 파티클 위치를 십자로 그린다. 바운드는 엔진의 Show Bounds가 컴포넌트 단위로 그린다.
	void DrawDebug(FLineBatcher& LineBatcher) const;

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

public:
	UParticleSystem* Template = nullptr;
	// BeginPlay에서 자동으로 시작할지
	bool bAutoActivate = true;
	// 모든 이미터에 추가로 거는 지연 (초)
	float EmitterDelay = 0.0f;
	// 0이면 시작할 때마다 다른 시드
	int32 RandomSeed = 0;

private:
	TArray<FParticleEmitterInstance*> EmitterInstances;
	FRandomStream RandomStream;

	// 등록된 뒤 아직 Tick하지 않음 (UE와 같은 이름)
	bool bJustRegistered = false;
	bool bWasDeactivated = false;
	bool bWasCompleted = false;
	// 한 번이라도 활성화됐는지. 게임에서 다시 켤 때 전부 초기화할지 판단한다.
	bool bHasBeenActivated = false;
};
