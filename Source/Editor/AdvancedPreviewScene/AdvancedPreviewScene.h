#pragma once
#include "Engine/PreviewScene.h"

class AStaticMeshActor;

// 프리뷰 씬 + 바닥 (UE FAdvancedPreviewScene)
class FAdvancedPreviewScene : public FPreviewScene
{
public:
	static constexpr float FloorSize = 40.0f;     // 한 변 (m)
	static constexpr float FloorThickness = 1.0f; // 두께 (m)

	// InFloorHeight: 바닥 윗면의 Z
	explicit FAdvancedPreviewScene(float InFloorHeight = 0.0f);

	void SetFloorVisible(bool bVisible);
	bool IsFloorVisible() const { return bShowFloor; }
	float GetFloorHeight() const { return FloorHeight; }

private:
	AStaticMeshActor* FloorActor = nullptr; // 소유는 PreviewWorld
	float FloorHeight = 0.0f;
	bool bShowFloor = false;
};
