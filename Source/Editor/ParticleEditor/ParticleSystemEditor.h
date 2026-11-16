#pragma once
#include "Editor/ParticleEditor/ParticleSystemPreviewScene.h"
#include "Editor/Toolkits/AssetEditorToolkit.h"

class UParticleSystem;

// 파티클 시스템 에셋 하나를 여는 창 (UE FCascade)
// 지금은 프리뷰 뷰포트만 있다. Emitters / Details 패널은 자리만 둔다
class FParticleSystemEditor : public FAssetEditorToolkit
{
public:
	FParticleSystemEditor(UParticleSystem* InSystem, int32 InEditorId);

	// 루트 창과 전용 도킹 공간을 열고 뷰포트, 이미터, 디테일 패널을 그림
	void OnRender() override;

	URenderAsset* GetEditedAsset() const override;
	TUniquePtr<FAssetEditorToolkit> CreateReplacementEditor(int32 NewEditorId) const override;

protected:
	FAdvancedPreviewScene* GetPreviewScene() const override { return PreviewScene.get(); }
	// 이미터 수, 살아 있는 파티클 수
	FString GetViewportStats() const override;

private:
	UParticleSystem* System = nullptr; // 소유는 UAssetManager
	TUniquePtr<FParticleSystemPreviewScene> PreviewScene;

	FString WindowTitle;    // 파일 이름###에셋 키. 에셋별로 창 위치가 저장된다
	FString ViewportPanelTitle;
	FString EmittersPanelTitle;
	FString DetailsPanelTitle;
};
