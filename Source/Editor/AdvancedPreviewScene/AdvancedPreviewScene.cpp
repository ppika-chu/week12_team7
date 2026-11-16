#include "EnginePCH.h"

#include "Editor/AdvancedPreviewScene/AdvancedPreviewScene.h"

#include "Asset/AssetManager.h"
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
}

void FAdvancedPreviewScene::Tick(float DeltaTime)
{
	GetWorld()->Tick(DeltaTime);
}

void FAdvancedPreviewScene::SetFloorVisible(bool bVisible)
{
	bShowFloor = bVisible;
	FloorActor->GetStaticMeshComponent()->SetVisible(bShowFloor);
}
