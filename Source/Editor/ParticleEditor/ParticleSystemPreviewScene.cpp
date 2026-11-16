#include "EnginePCH.h"

#include "Editor/ParticleEditor/ParticleSystemPreviewScene.h"

#include "Components/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Particles/ParticleSystem.h"

FParticleSystemPreviewScene::FParticleSystemPreviewScene(UParticleSystem* InSystem)
	: System(InSystem)
{
	// Persona와 같이 빈 액터를 원점에 두고 컴포넌트를 루트로 붙인다. 등록되면서 기본 템플릿으로 한 번 켜진다
	PreviewActor = GetWorld()->SpawnActor<AActor>(NAME_None);
	ParticleComponent = Cast<UParticleSystemComponent>(PreviewActor->AddComponent(UParticleSystemComponent::StaticClass(), FName("PreviewParticles")));
	// 편집할 에셋으로 바꾼다. 켜져 있던 상태라 바로 다시 켜진다
	ParticleComponent->SetTemplate(System);
}

void FParticleSystemPreviewScene::RestartSimulation()
{
	// 인스턴스까지 지운 뒤 켜면 InitializeSystem부터 다시 한다
	ParticleComponent->ResetParticles(true);
	ParticleComponent->Activate(true);
}

float FParticleSystemPreviewScene::GetSceneRadius() const
{
	const float HalfFloor = FloorSize * 0.5f;
	return FVector(HalfFloor, HalfFloor, GetFloorHeight() - FloorThickness).Size();
}
