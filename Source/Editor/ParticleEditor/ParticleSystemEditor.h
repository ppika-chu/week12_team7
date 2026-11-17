#pragma once
#include "Editor/ParticleEditor/ParticleSystemPreviewScene.h"
#include "Editor/Toolkits/AssetEditorToolkit.h"

class UParticleSystem;
class UParticleEmitter;
class UParticleModule;
class UClass;
class UTexture2D;

// 파티클 시스템 에셋 하나를 여는 창 (UE FCascade)
class FParticleSystemEditor : public FAssetEditorToolkit
{
public:
	FParticleSystemEditor(UParticleSystem* InSystem, int32 InEditorId);

	// 장면 Tick 후 Loop 처리: 시스템이 끝나면 0.5초 뒤 다시 재생 (UE FCascade::Tick)
	void Tick(float DeltaTime) override;

	// 루트 창과 전용 도킹 공간을 열고 뷰포트, 이미터, 디테일 패널을 그림
	void OnRender() override;

	URenderAsset* GetEditedAsset() const override;
	TUniquePtr<FAssetEditorToolkit> CreateReplacementEditor(int32 NewEditorId) const override;

	// Bounds 상자
	void DrawPreviewOverlay(FLineBatcher& LineBatcher) const override;

protected:
	FAdvancedPreviewScene* GetPreviewScene() const override { return PreviewScene.get(); }
	// 뷰포트 위 View 메뉴 + 재생 / Loop 버튼 + 오른쪽 속도
	void DrawViewportToolbar() override;
	// 이미터 수, 살아 있는 파티클 수. View > Particle Counts가 꺼지면 비운다
	FString GetViewportStats() const override;

private:
	// 창 맨 위 툴바: Restart Sim, Bounds, Origin Axis (Cascade 상단 툴바)
	void DrawToolbar();
	// 이미터 하나 = 열 하나. 헤더 아래에 TypeData, Required, Spawn, 일반 모듈 순 (UE FCascadeEmitterCanvasClient)
	void DrawEmitterCanvas();
	// 선택한 모듈 > 이미터 > 파티클 시스템 순으로 하나를 골라 리플렉션 속성을 그린다 (UE FCascade::SetSelection)
	void DrawDetails();
	// 편집 뒤 프리뷰와 레벨에서 같은 에셋을 쓰는 컴포넌트를 모두 다시 시작한다 (UE FCascade::OnRestartInLevel)
	void OnRestartInLevel();
	// 모듈 한 줄: [켜기] 이름. 클릭하면 선택. Required / TypeData는 체크박스 없음. X, Y는 화면 좌표
	void DrawModuleBlock(UParticleEmitter* Emitter, UParticleModule* Module, float X, float Y, float Height, bool bDrawEnableButton);

	// 우클릭 메뉴. 모듈 / 이미터 / 빈 곳마다 다르다 (UE FCascadeEmitterCanvasClient::BuildMenuWidget*)
	void RequestContextMenu(UParticleEmitter* Emitter, UParticleModule* Module);
	void DrawCanvasContextMenu();
	void DrawEmitterMenu(UParticleEmitter* Emitter);
	void DrawModuleMenu(UParticleEmitter* Emitter, UParticleModule* Module);

	// 구조 편집 (UE FCascade::OnNewEmitter, OnDuplicateEmitter, OnDeleteEmitter, OnNewModule, OnDeleteModule)
	void NewEmitter(int32 InsertIndex);
	void DuplicateEmitter(UParticleEmitter* Emitter);
	void DeleteEmitter(UParticleEmitter* Emitter);
	void NewModule(UParticleEmitter* Emitter, UClass* ModuleClass);
	void DeleteModule(UParticleEmitter* Emitter, UParticleModule* Module);
	// 구조가 바뀐 뒤 모듈 목록과 메모리 배치를 다시 만들고 모든 컴포넌트를 다시 시작한다 (UE FCascade::ForceUpdate)
	void ForceUpdate();

	UParticleSystem* System = nullptr; // 소유는 UAssetManager
	TUniquePtr<FParticleSystemPreviewScene> PreviewScene;

	FString WindowTitle;    // 파일 이름###에셋 키. 에셋별로 창 위치가 저장된다
	FString ViewportPanelTitle;
	FString EmittersPanelTitle;
	FString DetailsPanelTitle;

	bool bShowBounds = false;
	bool bShowOriginAxis = false;
	bool bShowParticleCounts = true;

	// Emitters 패널에서 고른 대상. Details가 이것을 보여 준다 (UE FCascade::SelectedEmitter / SelectedModule)
	UParticleEmitter* SelectedEmitter = nullptr;
	UParticleModule* SelectedModule = nullptr;

	// 우클릭한 대상. 둘 다 nullptr이면 빈 곳
	UParticleEmitter* ContextEmitter = nullptr;
	UParticleModule* ContextModule = nullptr;
	bool bContextMenuRequested = false;

	// Cascade Loop. 끝난 시스템을 자동으로 다시 재생할지. 이미터 반복(EmitterLoops)과는 다르다
	bool bLoopSystem = true;
	bool bPendingReset = false;
	double TotalTime = 0.0;   // 시간 배율이 곱해진 프리뷰 누적 시간 (s). 멈추면 늘지 않는다
	double ResetTime = 0.0;   // 이 시간이 지나면 다시 재생

	// 애니메이션 에디터와 같은 재생 아이콘. 소유는 UAssetManager
	UTexture2D* PlayIcon = nullptr;
	UTexture2D* PauseIcon = nullptr;
	UTexture2D* LoopingIcon = nullptr;
	UTexture2D* NoLoopingIcon = nullptr;
};
