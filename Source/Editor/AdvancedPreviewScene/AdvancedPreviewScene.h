#pragma once
#include "Engine/PreviewScene.h"

class AStaticMeshActor;
class UBoxComponent;

// 프리뷰 씬 + 바닥 + 월드 Tick (UE FAdvancedPreviewScene)
class FAdvancedPreviewScene : public FPreviewScene
{
public:
	static constexpr float FloorSize = 40.0f;     // 한 변 (m)
	static constexpr float FloorThickness = 1.0f; // 두께 (m)

	// InFloorHeight: 바닥 윗면의 Z
	explicit FAdvancedPreviewScene(float InFloorHeight = 0.0f);

	// 월드를 Tick한다. UE는 FTickableEditorObject라 엔진이 자동으로 부르지만, 여기서는 에디터가 부른다
	virtual void Tick(float DeltaTime);

	void SetFloorVisible(bool bVisible);
	bool IsFloorVisible() const { return bShowFloor; }
	float GetFloorHeight() const { return FloorHeight; }

private:
	AStaticMeshActor* FloorActor = nullptr; // 소유는 PreviewWorld
	// 바닥 충돌. 파티클 충돌은 Block ShapeComponent만 검사한다
	UBoxComponent* FloorCollision = nullptr;
	float FloorHeight = 0.0f;
	bool bShowFloor = false;
};
