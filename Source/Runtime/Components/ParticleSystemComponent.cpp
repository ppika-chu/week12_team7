#include "EnginePCH.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "ParticleSystemComponent.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	// 파티클 시뮬레이션을 매 프레임 진행한다.
	PrimaryComponentTick.bCanEverTick = true;

	// 배치하자마자 보이도록 기본 에셋으로 시작한다.
	Template = UParticleSystem::GetDefault();
}

void UParticleSystemComponent::OnRegister()
{
	Super::OnRegister();

	// Template이 없는 컴포넌트는 아무것도 만들지 않는다.
	if (!Template)
	{
		return;
	}

	InitParticles();
}

void UParticleSystemComponent::OnUnregister()
{
	ResetParticles();
	Super::OnUnregister();
}

//이미터마다 인스턴스 생성
void UParticleSystemComponent::InitParticles()
{
	ResetParticles();

	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		FParticleEmitterInstance* Instance = new FParticleEmitterInstance(*this);
		Instance->InitParameters(Emitter);
		Instance->Init();
		EmitterInstances.Add(Instance);
	}
}

void UParticleSystemComponent::ResetParticles()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}

	EmitterInstances.Reset();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	//인스턴스를 순회하며 Tick 호출
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		// TODO : bSupressSpawning = 새 파티클은 만들지 말고, 살아 있는 것만 계속 시뮬레이션하라
		Instance->Tick(DeltaTime, false);
	}
}
