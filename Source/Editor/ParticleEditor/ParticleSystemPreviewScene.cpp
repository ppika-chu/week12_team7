#include "EnginePCH.h"

#include "Editor/ParticleEditor/ParticleSystemPreviewScene.h"

#include "Components/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemLoader.h"

FParticleSystemPreviewScene::FParticleSystemPreviewScene(UParticleSystem* InSystem)
	: System(InSystem)
{
	// Persona와 같이 빈 액터를 원점에 두고 컴포넌트를 루트로 붙인다. 등록되면서 기본 템플릿으로 한 번 켜진다
	PreviewActor = GetWorld()->SpawnActor<AActor>(NAME_None);
	ParticleComponent = Cast<UParticleSystemComponent>(PreviewActor->AddComponent(UParticleSystemComponent::StaticClass(), FName("PreviewParticles")));
	// 편집할 에셋으로 바꾼다. 켜져 있던 상태라 바로 다시 켜진다
	ParticleComponent->SetTemplate(System);
	ParticleComponent->SetPreviewLODIndex(0);
}

void FParticleSystemPreviewScene::RestartSimulation()
{
	FString Error;
	if (!FParticleSystemLoader::ValidateAsset(System, Error))
	{
		ParticleComponent->ResetParticles(true);
		HTR_LOG(Warning, "[Particle] Preview restart failed: {}", Error);
		return;
	}
	// UE FCascade::RestartParticleSystem과 같다. 파티클만 지우고 인스턴스는 남겨 시간만 되감는다
	ParticleComponent->ResetParticles();
	ParticleComponent->ActivateSystem();
}

void FParticleSystemPreviewScene::SetLODIndex(int32 LODIndex)
{
	ParticleComponent->SetPreviewLODIndex(LODIndex);
}

int32 FParticleSystemPreviewScene::GetLODIndex() const
{
	return ParticleComponent->GetPreviewLODIndex();
}

void FParticleSystemPreviewScene::SetPaused(bool bInPaused)
{
	bPaused = bInPaused;
	ApplyTimeScale();
}

void FParticleSystemPreviewScene::SetSimulationSpeed(float InSpeed)
{
	SimulationSpeed = InSpeed;
	ApplyTimeScale();
}

void FParticleSystemPreviewScene::ApplyTimeScale()
{
	// 0 ~ 10배로 제한된다
	GetWorld()->SetTimeScale(GetTimeScale());
}

float FParticleSystemPreviewScene::GetSceneRadius() const
{
	const float HalfFloor = FloorSize * 0.5f;
	return FVector(HalfFloor, HalfFloor, GetFloorHeight() - FloorThickness).Size();
}
