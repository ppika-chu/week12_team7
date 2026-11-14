#pragma once

#include "Components/PrimitiveComponent.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Containers/Array.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;
struct FPrimitiveSceneProxy;

UCLASS()
class UFXSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()
};

UCLASS()
class UParticleSystemComponent : public UFXSystemComponent
{
	GENERATED_BODY()

public:
	UParticleSystemComponent();
	void UpdateDynamicData();
	const TArray<FDynamicEmitterDataBase*>& GetEmitterRenderData() const { return EmitterRenderData; }

	// 설계도(템플릿). 비어 있을 수 있으므로 쓰는 쪽에서 nullptr를 확인한다.
	UParticleSystem* Template;

	//~Start AActorComponent Interface
	virtual void TickComponent(float DeltaTime) override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	//~End of AActorComponent Interface

	//~Start UPrimitiveComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// 화면 밖 판정(컬링)에 쓰는 상자. 메시가 없으므로 직접 정한다.
	virtual FBox CalcLocalBounds() const override;
	//~End of UPrimitiveComponent Interface


protected:
	void InitParticles();
	void ResetParticles();


	// 시뮬레이션 실행 실체
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// 렌더 데이터
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;
};