#include "EnginePCH.h"

#include "Editor/Persona/AnimationEditorPreviewScene.h"

#include "Components/SkeletalMeshComponent.h"
#include "DebugSkelMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Render/Mesh.h"
#include "Render/SkeletalMesh.h"

#include <cfloat>
#include <cmath>

FAnimationEditorPreviewScene::FAnimationEditorPreviewScene(USkeletalMesh* InMesh)
	: FAdvancedPreviewScene(ComputeFloorHeight(InMesh))
	, Mesh(InMesh)
{
	CreatePreviewActor();
	SceneRadius = ComputeSceneRadius();
}

void FAnimationEditorPreviewScene::Tick(float DeltaTime)
{
	FAdvancedPreviewScene::Tick(DeltaTime);

	// 월드 Tick에서 모은 Root Motion으로 프리뷰 메시를 옮긴다. 바닥 밖은 반대편으로 넘긴다 (UE FAnimationEditorPreviewScene::Tick)
	if (PreviewComponent)
	{
		const float HalfFloor = FloorSize * 0.5f;
		PreviewComponent->ConsumeRootMotion(FVector(-HalfFloor, -HalfFloor, 0.0f), FVector(HalfFloor, HalfFloor, 0.0f));
	}
}

void FAnimationEditorPreviewScene::SetMeshVisible(bool bVisible)
{
	bShowMesh = bVisible;
	PreviewComponent->SetVisible(bShowMesh);
}

void FAnimationEditorPreviewScene::SetAnimationPlaybackSpeedMode(EAnimationPlaybackSpeeds::Type InMode)
{
	AnimationPlaybackSpeedMode = InMode;

	if (UWorld* PreviewWorld = GetWorld())
	{
		const float AnimationSpeed = (InMode == EAnimationPlaybackSpeeds::Custom)
			? GetCustomAnimationSpeed()
			: EAnimationPlaybackSpeeds::Values[AnimationPlaybackSpeedMode];
		// 0 ~ 10배로 제한된다
		PreviewWorld->SetTimeScale(AnimationSpeed);
	}
}

void FAnimationEditorPreviewScene::SetCustomAnimationSpeed(float InAnimationSpeed)
{
	CustomAnimationSpeed = InAnimationSpeed;
	SetAnimationPlaybackSpeedMode(EAnimationPlaybackSpeeds::Custom);
}

void FAnimationEditorPreviewScene::ComputeFocus(FVector& OutTarget, float& OutDistance) const
{
	// Bind Pose 메시 높이의 가운데를 본다. 정점이 없으면 키 1.7m 기준
	const TArray<FVertex>& Vertices = Mesh->GetMeshData().Vertices;
	float MinZ = 0.0f;
	float MaxZ = 1.7f;
	if (Vertices.Num() > 0)
	{
		MinZ = FLT_MAX;
		MaxZ = -FLT_MAX;
		for (const FVertex& Vertex : Vertices)
		{
			MinZ = Vertex.Position.Z < MinZ ? Vertex.Position.Z : MinZ;
			MaxZ = Vertex.Position.Z > MaxZ ? Vertex.Position.Z : MaxZ;
		}
	}

	const float Height = MaxZ - MinZ > 0.01f ? MaxZ - MinZ : 0.01f;
	OutTarget = FVector(0.0f, 0.0f, (MinZ + MaxZ) * 0.5f);
	OutDistance = Height * 2.0f;
}

void FAnimationEditorPreviewScene::CreatePreviewActor()
{
	// 언리얼 PersonaToolkit처럼 빈 액터를 원점에 두고 스켈레탈 메시 컴포넌트를 루트로 붙인다
	// 액터에 루트가 없으면 AddComponent가 루트로 삼는다. SetSkeletalMesh가 포즈를 Bind Pose로 초기화한다
	PreviewActor = GetWorld()->SpawnActor<AActor>(NAME_None);
	PreviewComponent = Cast<UDebugSkelMeshComponent>(PreviewActor->AddComponent(UDebugSkelMeshComponent::StaticClass(), FName("PreviewMesh")));
	PreviewComponent->SetSkeletalMesh(Mesh);
	PreviewComponent->SetVisible(bShowMesh);
}

float FAnimationEditorPreviewScene::ComputeFloorHeight(const USkeletalMesh* InMesh)
{
	const TArray<FVertex>& Vertices = InMesh->GetMeshData().Vertices;
	if (Vertices.Num() == 0)
	{
		return 0.0f;
	}

	float MinZ = FLT_MAX;
	for (const FVertex& Vertex : Vertices)
	{
		MinZ = Vertex.Position.Z < MinZ ? Vertex.Position.Z : MinZ;
	}
	return MinZ;
}

float FAnimationEditorPreviewScene::ComputeSceneRadius() const
{
	// 바닥 아랫면 귀퉁이까지의 거리로 시작한다
	const float HalfFloor = FloorSize * 0.5f;
	float Radius = FVector(HalfFloor, HalfFloor, GetFloorHeight() - FloorThickness).Size();
	for (const FVertex& Vertex : Mesh->GetMeshData().Vertices)
	{
		const float Distance = Vertex.Position.Size();
		Radius = Distance > Radius ? Distance : Radius;
	}
	return Radius;
}
