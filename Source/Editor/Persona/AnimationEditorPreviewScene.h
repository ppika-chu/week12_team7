#pragma once
#include "Editor/AdvancedPreviewScene/AdvancedPreviewScene.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Render/RenderingInfo.h"
#include "Render/Texture2D.h"

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

// 에셋 에디터의 3D 화면. 언리얼 FAnimationEditorPreviewScene에 해당한다.
// 월드·라이트·바닥은 부모가 만들고, 여기서는 스켈레탈 메시와 렌더 타깃, 오빗 카메라를 가진다. 에셋을 고치지 않는다.
class FAnimationEditorPreviewScene : public FAdvancedPreviewScene
{
public:
	explicit FAnimationEditorPreviewScene(USkeletalMesh* InMesh);

	// 월드 Tick 후 렌더 타깃과 카메라를 맞춘다. 렌더가 Tick 뒤에 오므로 여기서 한다
	void Tick(float DeltaTime) override;

	// 프리뷰 이미지 위의 마우스 입력으로 오빗 값을 갱신한다. 프리뷰 이미지 위 투명 버튼 다음에 호출한다
	void HandleCameraInput(bool bAllowOrbit);
	// 메시 높이 가운데를 대상으로, 키의 2배 거리에서 보도록 초기화
	void FocusOnMesh();

	// 뷰포트 칸 크기를 기록한다. 다음 Tick에서 타깃을 맞춘다 (1프레임 지연)
	void SetDesiredSize(uint32 Width, uint32 Height) { DesiredWidth = Width; DesiredHeight = Height; }

	void SetMeshVisible(bool bVisible);
	bool IsMeshVisible() const { return bShowMesh; }

	// 프리뷰 월드 시간 배율로 재생 속도를 바꾼다 (언리얼은 WorldSettings의 TimeDilation)
	void SetAnimationPlaybackSpeedMode(EAnimationPlaybackSpeeds::Type InMode);
	EAnimationPlaybackSpeeds::Type GetAnimationPlaybackSpeedMode() const { return AnimationPlaybackSpeedMode; }
	// 값을 저장하고 Custom 모드로 바꾼다
	void SetCustomAnimationSpeed(float InAnimationSpeed);
	float GetCustomAnimationSpeed() const { return CustomAnimationSpeed; }

	UDebugSkelMeshComponent* GetPreviewComponent() const { return PreviewComponent; }
	const FEditorViewportClient& GetViewportClient() const { return ViewportClient; }
	FEditorViewportClient& GetViewportClient() { return ViewportClient; }
	const FRenderingInfo& GetRenderingInfo() const { return RenderingInfo; }
	FTexture2D* GetColorTarget() const { return ColorTarget.get(); }
	FTexture2D* GetHDRTarget() const { return HDRTarget.get(); }
	uint32 GetTargetWidth() const { return TargetWidth; }
	uint32 GetTargetHeight() const { return TargetHeight; }

private:
	// 원점에 프리뷰 액터와 스켈레탈 메시 컴포넌트 배치
	void CreatePreviewActor();
	// 메시에서 가장 낮은 정점의 Z. 바닥 높이 (부모 생성자에 넘기므로 static)
	static float ComputeFloorHeight(const USkeletalMesh* InMesh);
	// 원점에서 바닥 귀퉁이와 메시 정점 중 가장 먼 거리. 카메라 Far 계산에 쓴다
	float ComputeSceneRadius() const;
	// 뷰포트 칸 크기가 바뀌었으면 렌더 타깃을 다시 만든다
	void UpdateRenderTargets();
	// 오빗 값(Target, Yaw, Pitch, Distance)으로 카메라 위치와 방향 설정
	void UpdatePreviewCamera();

	USkeletalMesh* Mesh = nullptr; // 소유는 UAssetManager

	// 원점에 둔 빈 액터 + 스켈레탈 메시 컴포넌트 (언리얼 AAnimationEditorPreviewActor). 소유는 PreviewWorld
	AActor* PreviewActor = nullptr;
	UDebugSkelMeshComponent* PreviewComponent = nullptr;
	bool bShowMesh = true;
	EAnimationPlaybackSpeeds::Type AnimationPlaybackSpeedMode = EAnimationPlaybackSpeeds::Normal;
	float CustomAnimationSpeed = 1.0f;

	// 레벨 뷰포트 슬롯 / PIE 창과 같은 구성 (EditorEngine::UpdatePIEWindowTargets)
	FEditorViewportClient ViewportClient;
	TUniquePtr<FTexture2D> ColorTarget;     // 톤 매핑 결과. ImGui::Image로 표시
	TUniquePtr<FTexture2D> DepthTarget;
	TUniquePtr<FTexture2D> HDRTarget;       // 장면을 먼저 그리는 곳
	FRenderingInfo RenderingInfo{};
	uint32 TargetWidth = 0;
	uint32 TargetHeight = 0;
	uint32 DesiredWidth = 0;                // 지난 프레임 뷰포트 칸 크기
	uint32 DesiredHeight = 0;

	// 오빗 카메라. 각도는 도 단위, 바라보는 방향. Yaw -90 = +Y 쪽에서 -Y를 바라봄 (언리얼 Persona 기본)
	FVector OrbitTarget{ 0.0f, 0.0f, 0.0f };
	float OrbitYaw = -90.0f;
	float OrbitPitch = -15.0f;
	float OrbitDistance = 3.0f;               // m
	float SceneRadius = 0.0f;                 // m. 생성자에서 한 번 구한다
};
