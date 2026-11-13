#pragma once

#include "Components/PrimitiveComponent.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Containers/Array.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;

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

	// 설계도(템플릿). 비어 있을 수 있으므로 쓰는 쪽에서 nullptr를 확인한다.
	UParticleSystem* Template;

	//~Start AActorComponent Interface
	virtual void TickComponent(float DeltaTime) override;
	virtual void OnRegister() override;
	//~End of AActorComponent Interface

protected:
	// 시뮬레이션 실행 실체
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// 렌더 데이터
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;
};