#include "EnginePCH.h"
#include "ParticleSystemComponent.h"
#include "Asset/AssetManager.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include <algorithm>
#include <cmath>
#include <utility>

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCastShadow(false);
	SetReceiveShadow(false);
	SetReceivesDecals(false);
	Template = UAssetManager::GetAssetByPath<UParticleSystem>("DefaultParticleSystem");
}

// ************************************************************************
// 왜 OnRegister에서 초기화하는가?
// 장기적으로 Editor에서도 Tick할수 있어야 하기 때문
// 일반적으로 초기화를 하는 BeginPlay에서 초기화하면 Editor에서 Tick이 안됨
// ************************************************************************
void UParticleSystemComponent::OnRegister()
{
	Super::OnRegister();
	ResetParticles();

	// 게임 시작 후 재등록된 컴포넌트의 Tick 복구
	if (GetOwner() && GetOwner()->HasActorBegunPlay())
	{
		PrimaryComponentTick.RegisterTickFunction(GetWorld()->GetTickTaskManager());
	}
}

void UParticleSystemComponent::OnUnregister()
{
	PrimaryComponentTick.UnRegisterTickFunction();
	EmitterInstances.Reset();
	LocalBounds = FBox{ FVector::ZeroVector, FVector::ZeroVector };
	Super::OnUnregister();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (Template == InTemplate)
	{
		return;
	}
	Template = InTemplate;
	ResetParticles();
}

void UParticleSystemComponent::ResetParticles()
{
	EmitterInstances.Reset();
	MaxParticles = std::clamp(MaxParticles, 1, FParticleDataContainer::MaxCapacity);
	if (IsRegistered() && Template)
	{
		for (UParticleEmitter* Emitter : Template->GetEmitters())
		{
			auto Instance = MakeUnique<FParticleEmitterInstance>();
			if (Instance->Init(Emitter, MaxParticles))
			{
				EmitterInstances.Add(std::move(Instance));
			}
		}
	}
	SendRenderDynamicData();
}

int32 UParticleSystemComponent::GetNumActiveParticles() const
{
	int32 Count = 0;
	for (const auto& Instance : EmitterInstances)
	{
		Count += Instance->ActiveParticles;
	}
	return Count;
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	if (!IsActive())
	{
		return;
	}
	for (const auto& Instance : EmitterInstances)
	{
		Instance->Tick(DeltaTime);
	}
	SendRenderDynamicData();
}

TArray<FDynamicSpriteEmitterReplayData> UParticleSystemComponent::BuildReplayData()
{
	TArray<FDynamicSpriteEmitterReplayData> ReplayData;
	LocalBounds = FBox{ FVector::ZeroVector, FVector::ZeroVector };
	const FVector Scale = GetWorldScale3D().GetAbs();

	// 펼쳐진 스프라이트의 bounds를 표현할 수 없는 0 스케일 제외
	if (Scale.X == 0.0f || Scale.Y == 0.0f || Scale.Z == 0.0f)
	{
		return ReplayData;
	}

	bool bHasBounds = false;
	ReplayData.SetNum(EmitterInstances.Num(), false);
	for (int32 Index = 0; Index < EmitterInstances.Num(); ++Index)
	{
		auto& EmitterData = ReplayData[Index];
		EmitterInstances[Index]->FillReplayData(EmitterData, Scale);
		for (const FParticleSpriteInstanceData& Sprite : EmitterData.Particles)
		{
			// 카메라 방향과 회전에 관계없이 감싸는 반지름
			const float Radius = 0.5f * std::sqrt(Sprite.Size.X * Sprite.Size.X + Sprite.Size.Y * Sprite.Size.Y);
			const FVector HalfExtent(Radius / Scale.X, Radius / Scale.Y, Radius / Scale.Z);
			const FBox ParticleBounds{ Sprite.Center - HalfExtent, Sprite.Center + HalfExtent };
			if (bHasBounds)
			{
				LocalBounds.Expand(ParticleBounds);
			}
			else
			{
				LocalBounds = ParticleBounds;
				bHasBounds = true;
			}
		}
	}
	return ReplayData;
}

void UParticleSystemComponent::SendRenderDynamicData()
{
	auto ReplayData = BuildReplayData();
	if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
	{
		static_cast<FParticleSystemSceneProxy*>(Proxy)->SetDynamicData(std::move(ReplayData));
	}

	// 바뀐 파티클 범위를 다음 프러스텀 판정에 반영
	Super::OnTransformDirty();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	auto* Proxy = new FParticleSystemSceneProxy(this);
	Proxy->SetDynamicData(BuildReplayData());
	return Proxy;
}

void UParticleSystemComponent::OnTransformDirty()
{
	SendRenderDynamicData();
}

bool UParticleSystemComponent::LineTraceComponentLocal(const FRay& LocalRay, float& OutT)
{
	float HitT;
	if (!RayIntersectsAABB(LocalRay, LocalBounds.Min, LocalBounds.Max, HitT) || HitT >= OutT)
	{
		return false;
	}
	OutT = HitT;
	return true;
}

void UParticleSystemComponent::AddReferencedMaterials(TSet<UMaterial*>& OutMaterials) const
{
	Super::AddReferencedMaterials(OutMaterials);
	if (!Template)
	{
		return;
	}
	for (UParticleEmitter* Emitter : Template->GetEmitters())
	{
		for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
		{
			if (LOD->RequiredModule)
			{
				OutMaterials.Add(LOD->RequiredModule->Material);
			}
		}
	}
}

void UParticleSystemComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "MaxParticles")
	{
		ResetParticles();
	}
}