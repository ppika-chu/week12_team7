#include "EnginePCH.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "ParticleSystemComponent.h"
#include "Particles/ParticleEmitterInstances.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Engine/PrimitiveSceneProxy.h"

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

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	FParticleSystemSceneProxy* Proxy = new FParticleSystemSceneProxy(this);
	return Proxy;
}

FBox UParticleSystemComponent::CalcLocalBounds() const
{
	// TODO(임시): 원본은 UpdateBoundingBox가 파티클 위치로 계산한 상자를 쓴다.
	// 그 계산이 들어오기 전까지 넉넉한 고정 상자를 써서, 이미터 원점이 화면 밖이어도 파티클이 잘리지 않게 한다.
	const float HalfExtent = 10.0f;
	return FBox{ FVector(-HalfExtent, -HalfExtent, -HalfExtent), FVector(HalfExtent, HalfExtent, HalfExtent) };
}

//이미터마다 인스턴스 생성
void UParticleSystemComponent::InitParticles()
{
	ResetParticles();

	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		FParticleEmitterInstance* Instance = new FParticleSpriteEmitterInstance(*this);
		Instance->InitParameters(Emitter);
		Instance->Init();
		EmitterInstances.Add(Instance);
	}
}

void UParticleSystemComponent::ResetParticles()
{
	for (int32 Index = 0; Index < EmitterRenderData.Num(); ++Index)
	{
		FDynamicEmitterDataBase* Data = EmitterRenderData[Index];
		delete Data;
	}
	EmitterRenderData.Reset();

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}

	EmitterInstances.Reset();
}

void UParticleSystemComponent::UpdateDynamicData()
{
	//이전 프레임 렌더 데이터 리셋
	for (int32 Index = 0; Index < EmitterRenderData.Num(); ++Index)
	{
		FDynamicEmitterDataBase* Data = EmitterRenderData[Index];
		delete Data;
	}
	EmitterRenderData.Reset();

	for (int32 EmitterIndex = 0; EmitterIndex < EmitterInstances.Num(); EmitterIndex++)
	{
		FDynamicEmitterDataBase* NewDynamicEmitterData = NULL;
		FParticleEmitterInstance* EmitterInst = EmitterInstances[EmitterIndex];
		if (EmitterInst)
		{
			NewDynamicEmitterData = EmitterInst->GetDynamicData(false);
			if (NewDynamicEmitterData != NULL)
			{
				NewDynamicEmitterData->bValid = true;
				EmitterRenderData.Add(NewDynamicEmitterData);
				NewDynamicEmitterData->EmitterIndex = EmitterIndex;
			}
		}
	}
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

	// TODO(디버그): 3·4단계 확인용 로그. 1초에 한 번만 찍는다. 확인 후 삭제.
	//static float DebugLogTimer = 0.f;
	//DebugLogTimer += DeltaTime;
	//if (DebugLogTimer >= 1.f)
	//{
	//	DebugLogTimer = 0.f;
	//	for (int32 Index = 0; Index < EmitterInstances.Num(); ++Index)
	//	{
	//		const FParticleEmitterInstance* Instance = EmitterInstances[Index];
	//		HTR_LOG(Info, "[Particle] Emitter={} Active={} Max={} Stride={} RenderData={}",
	//			Index, Instance->ActiveParticles, Instance->MaxActiveParticles, Instance->ParticleStride, EmitterRenderData.Num());
	//	}
	//	for (const FDynamicEmitterDataBase* Data : EmitterRenderData)
	//	{
	//		const FDynamicEmitterReplayDataBase& Source = Data->GetSource();
	//		HTR_LOG(Info, "[Particle] Copy: Emitter={} Active={} Stride={} MemBlock={}",
	//			Data->EmitterIndex, Source.ActiveParticleCount, Source.ParticleStride, Source.DataContainer.MemBlockSize);
	//	}
	//}
}
