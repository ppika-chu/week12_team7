#pragma once
#include "Editor/Gizmo/Gizmo.h"
#include "Editor/Viewports/EditorViewportClient.h"
#include "Render/RenderingInfo.h"
#include "Render/Texture2D.h"

// 에셋 에디터의 뷰포트. 렌더 타깃, 오빗 카메라, 기즈모를 가진다 (UE SEditorViewport + FEditorViewportClient).
// 무엇을 그리고 기즈모가 무엇을 움직일지는 모른다. 장면은 FPreviewScene, 기즈모 대상은 에디터가 정한다.
class FPreviewViewport
{
public:
	// 렌더 타깃 크기와 카메라를 맞춘다. 장면 Tick 다음, 렌더 전에 부른다
	void Tick();

	// 프리뷰 이미지 위의 마우스 입력으로 오빗 값을 갱신한다. 프리뷰 이미지 위 투명 버튼 다음에 호출한다
	void HandleCameraInput(bool bAllowOrbit);

	// 프리뷰 이미지 위 마우스로 기즈모 Hover / Drag를 갱신한다. 이미지에 hover 중이거나 드래그 중이면 true
	// MousePos는 이미지 왼쪽 위 기준 픽셀. 투명 버튼 다음에 호출한다. 결과는 GetGizmo().GetDragResult()
	bool UpdateGizmo(const FVector2& MousePos);
	FGizmo& GetGizmo() { return Gizmo; }
	const FGizmo& GetGizmo() const { return Gizmo; }

	// 대상 점과 거리로 카메라를 맞추고, F키로 돌아갈 값으로 기억한다
	void FocusOn(const FVector& Target, float Distance);
	// 원점에서 장면 끝까지 거리 (m). 카메라 Far 계산에 쓴다
	void SetSceneRadius(float Radius) { SceneRadius = Radius; }

	// 뷰포트 칸 크기를 기록한다. 다음 Tick에서 타깃을 맞춘다 (1프레임 지연)
	void SetDesiredSize(uint32 Width, uint32 Height) { DesiredWidth = Width; DesiredHeight = Height; }

	const FEditorViewportClient& GetViewportClient() const { return ViewportClient; }
	FEditorViewportClient& GetViewportClient() { return ViewportClient; }
	const FRenderingInfo& GetRenderingInfo() const { return RenderingInfo; }
	FTexture2D* GetColorTarget() const { return ColorTarget.get(); }
	FTexture2D* GetHDRTarget() const { return HDRTarget.get(); }
	uint32 GetTargetWidth() const { return TargetWidth; }
	uint32 GetTargetHeight() const { return TargetHeight; }

private:
	// 뷰포트 칸 크기가 바뀌었으면 렌더 타깃을 다시 만든다
	void UpdateRenderTargets();
	// 오빗 값(Target, Yaw, Pitch, Distance)으로 카메라 위치와 방향 설정
	void UpdateCamera();

	// 레벨 뷰포트 슬롯 / PIE 창과 같은 구성 (EditorEngine::UpdatePIEWindowTargets)
	FEditorViewportClient ViewportClient;
	// 레벨 에디터 기즈모와 따로 (UE FEditorViewportClient::Widget). Pivot이 없으면 그리지 않는다
	FGizmo Gizmo;
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
	float OrbitDistance = 3.0f;             // m
	float SceneRadius = 0.0f;               // m

	// F키로 돌아갈 값
	FVector FocusTarget{ 0.0f, 0.0f, 0.0f };
	float FocusDistance = 3.0f;
};
