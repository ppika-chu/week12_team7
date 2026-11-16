#pragma once
#include "Editor/Viewports/PreviewViewport.h"

class URenderAsset;
class UWorld;
class FPreviewScene;
class FLineBatcher;

// 에셋 하나를 여는 에디터 창의 공통 부모 (UE FAssetEditorToolkit).
// 매니저와 렌더링은 이 타입만 알면 된다. 장면은 파생 클래스가 FPreviewScene 파생으로 가진다.
class FAssetEditorToolkit
{
public:
	explicit FAssetEditorToolkit(int32 InEditorId);
	virtual ~FAssetEditorToolkit();

	FAssetEditorToolkit(const FAssetEditorToolkit&) = delete;
	FAssetEditorToolkit& operator=(const FAssetEditorToolkit&) = delete;

	// 안 보이면 skip. 장면 Tick → 뷰포트 Tick
	virtual void Tick(float DeltaTime);
	// 루트 창과 패널을 그린다
	virtual void OnRender() = 0;

	// 이 창이 여는 에셋. 같은 에셋이 이미 열려 있는지 비교할 때 씀
	virtual URenderAsset* GetEditedAsset() const = 0;
	// 재임포트로 에셋이 교체되었는지. 매니저가 CreateReplacementEditor를 부를지 정할 때 씀
	virtual bool IsEditedAssetRetired() const;
	// 재임포트로 에셋이 교체되었을 때 새 객체로 다시 연다. 새 객체가 없으면 nullptr
	virtual TUniquePtr<FAssetEditorToolkit> CreateReplacementEditor(int32 NewEditorId) const = 0;

	// X로 닫혔는지 확인. Manager가 삭제할 때 씀
	bool IsOpen() const { return bIsOpen; }
	// 지난 프레임에 포커스였는지. 레벨 단축키를 막을 때 씀
	bool IsFocused() const { return bIsFocused; }
	// 다음 프레임에 이 창을 앞으로 가져오도록 예약
	void Focus() { bFocusRequested = true; }

	// 이번 프레임에 프리뷰를 그려야 하는지. 뷰포트 패널이 보이고 렌더 타깃이 있을 때만
	bool ShouldRenderPreview() const { return bIsVisible && Viewport.GetColorTarget() != nullptr; }
	// UEditorEngine이 프리뷰 View를 그릴 때 쓰는 값들
	UWorld* GetPreviewWorld() const;
	const FEditorViewportClient& GetViewportClient() const { return Viewport.GetViewportClient(); }
	const FRenderingInfo& GetRenderingInfo() const { return Viewport.GetRenderingInfo(); }
	FTexture2D* GetHDRTarget() const { return Viewport.GetHDRTarget(); }
	// 레벨 View(1~5)와 겹치지 않는 그림자 / 라이트 캐시 키
	uint32 GetViewId() const { return 100u + static_cast<uint32>(EditorId); }
	// 톤 매핑 뒤 프리뷰 위에 그릴 선 (본 등). 그리기(OnRender)는 UEditorEngine이 한다
	virtual void DrawPreviewOverlay(FLineBatcher& LineBatcher) const {}
	// 뷰포트 기즈모. Pivot이 없으면 그리지 않는다. 그리기(FGizmoRenderer)는 UEditorEngine이 한다
	const FGizmo* GetGizmo() const { return &Viewport.GetGizmo(); }

protected:
	virtual FPreviewScene* GetPreviewScene() const = 0;

	// 툴바 + 프리뷰 이미지(왼쪽 위 통계) + 입력
	void DrawViewport();
	// 프리뷰 이미지 위 툴바. 기본은 없음
	virtual void DrawViewportToolbar() {}
	// 프리뷰 왼쪽 위에 겹쳐 그릴 글자. 비면 안 그린다
	virtual FString GetViewportStats() const { return FString(); }
	// 프리뷰 이미지 위의 입력. 기본은 카메라 조작만
	virtual void HandleViewportInput();

	// 에셋 키(폴더/이름.확장자)에서 폴더와 확장자를 뺀 이름. 창 제목에 보인다
	static FString GetDisplayName(const FString& AssetKey);

	int32 EditorId = 0;
	FPreviewViewport Viewport;

	bool bIsOpen = true;
	bool bIsVisible = false;    // 지난 프레임 뷰포트 패널의 Begin 결과
	bool bIsFocused = false;    // 지난 프레임에 루트 창이나 패널 중 하나가 포커스였는지
	bool bFocusRequested = true;
};
