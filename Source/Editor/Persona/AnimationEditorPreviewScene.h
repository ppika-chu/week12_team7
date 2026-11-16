#pragma once
#include "Editor/AdvancedPreviewScene/AdvancedPreviewScene.h"

class UDebugSkelMeshComponent;
class USkeletalMesh;
class AActor;

// 프리뷰 재생 속도. 언리얼 EAnimationPlaybackSpeeds에서 0.25, 0.75, 5배를 뺀 값만 둔다
namespace EAnimationPlaybackSpeeds
{
	enum Type
	{
		OneTenth = 0,
		Half,
		Normal,
		Double,
		TenTimes,
		Custom,
		NumPlaybackSpeeds
	};

	// Custom 칸은 쓰지 않는다. GetCustomAnimationSpeed 값을 쓴다
	inline constexpr float Values[NumPlaybackSpeeds] = { 0.1f, 0.5f, 1.0f, 2.0f, 10.0f, 1.0f };
}

// 스켈레탈 메시 에디터와 애니메이션 에디터의 프리뷰 장면 (UE FAnimationEditorPreviewScene).
// 월드·라이트·바닥은 부모가 만들고, 여기서는 스켈레탈 메시와 재생 속도를 가진다. 에셋을 고치지 않는다.
class FAnimationEditorPreviewScene : public FAdvancedPreviewScene
{
public:
	explicit FAnimationEditorPreviewScene(USkeletalMesh* InMesh);

	// 월드 Tick 후 Root Motion을 반영한다
	void Tick(float DeltaTime) override;

	// 메시 높이 가운데를 대상으로, 키의 2배 거리
	void ComputeFocus(FVector& OutTarget, float& OutDistance) const;
	// 원점에서 바닥 귀퉁이와 메시 정점 중 가장 먼 거리 (m)
	float GetSceneRadius() const { return SceneRadius; }

	void SetMeshVisible(bool bVisible);
	bool IsMeshVisible() const { return bShowMesh; }

	// 프리뷰 월드 시간 배율로 재생 속도를 바꾼다 (언리얼은 WorldSettings의 TimeDilation)
	void SetAnimationPlaybackSpeedMode(EAnimationPlaybackSpeeds::Type InMode);
	EAnimationPlaybackSpeeds::Type GetAnimationPlaybackSpeedMode() const { return AnimationPlaybackSpeedMode; }
	// 값을 저장하고 Custom 모드로 바꾼다
	void SetCustomAnimationSpeed(float InAnimationSpeed);
	float GetCustomAnimationSpeed() const { return CustomAnimationSpeed; }

	UDebugSkelMeshComponent* GetPreviewComponent() const { return PreviewComponent; }

private:
	// 원점에 프리뷰 액터와 스켈레탈 메시 컴포넌트 배치
	void CreatePreviewActor();
	// 메시에서 가장 낮은 정점의 Z. 바닥 높이 (부모 생성자에 넘기므로 static)
	static float ComputeFloorHeight(const USkeletalMesh* InMesh);
	float ComputeSceneRadius() const;

	USkeletalMesh* Mesh = nullptr; // 소유는 UAssetManager

	// 원점에 둔 빈 액터 + 스켈레탈 메시 컴포넌트 (언리얼 AAnimationEditorPreviewActor). 소유는 PreviewWorld
	AActor* PreviewActor = nullptr;
	UDebugSkelMeshComponent* PreviewComponent = nullptr;
	bool bShowMesh = true;
	EAnimationPlaybackSpeeds::Type AnimationPlaybackSpeedMode = EAnimationPlaybackSpeeds::Normal;
	float CustomAnimationSpeed = 1.0f;
	float SceneRadius = 0.0f; // m. 생성자에서 한 번 구한다
};
