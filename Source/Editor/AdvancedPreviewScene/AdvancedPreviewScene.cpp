#include "EnginePCH.h"

#include "Editor/AdvancedPreviewScene/AdvancedPreviewScene.h"

#include "Asset/AssetManager.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "Render/Mesh.h"

FAdvancedPreviewScene::FAdvancedPreviewScene(float InFloorHeight)
	: FloorHeight(InFloorHeight)
{
	// 윗면이 바닥 높이에 오도록 두께 절반만큼 내린다
	const FTransform FloorTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, FloorHeight - FloorThickness * 0.5f), FVector(FloorSize, FloorSize, FloorThickness));
	FloorActor = GetWorld()->SpawnActor<AStaticMeshActor>(NAME_None, &FloorTransform);
	// AStaticMeshActor::SetPrimitiveType은 비어 있어서 메시를 직접 지정한다
	FloorActor->GetStaticMeshComponent()->SetStaticMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Cube"));
	FloorActor->GetStaticMeshComponent()->SetVisible(bShowFloor);

	// 루트(Cube, 1m)에 붙어 스케일을 물려받으므로 Extent 0.5가 바닥 크기와 같다
	FloorCollision = Cast<UBoxComponent>(FloorActor->AddComponent(UBoxComponent::StaticClass(), FName("FloorCollision")));
	FloorCollision->SetBoxExtent(FVector(0.5f, 0.5f, 0.5f));
	FloorCollision->SetBlockComponent(bShowFloor);
}

void FAdvancedPreviewScene::Tick(float DeltaTime)
{
	GetWorld()->Tick(DeltaTime);
}

void FAdvancedPreviewScene::SetFloorVisible(bool bVisible)
{
	bShowFloor = bVisible;
	FloorActor->GetStaticMeshComponent()->SetVisible(bShowFloor);
	// 숨긴 바닥에 부딪히지 않게 표시와 함께 켜고 끈다
	FloorCollision->SetBlockComponent(bShowFloor);
}
