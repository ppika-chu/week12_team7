#include "EnginePCH.h"

#include "Editor/ParticleEditor/ParticleSystemEditor.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorUIScale.h"

#include "Asset/AssetManager.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"

#include <imgui.h>
#include <algorithm>
#include <format>

namespace
{
	// Cascade 기본 배치: 왼쪽 위 Viewport, 오른쪽 Emitters, 왼쪽 아래 Details
	void BuildDefaultPanelLayout(ImGuiID DockId, const ImVec2& Size, const char* ViewportPanel, const char* EmittersPanel, const char* DetailsPanel)
	{
		ImGui::DockBuilderAddNode(DockId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(DockId, Size);

		ImGuiID LeftId = DockId;
		const ImGuiID EmittersId = ImGui::DockBuilderSplitNode(LeftId, ImGuiDir_Right, 0.55f, nullptr, &LeftId);
		ImGuiID ViewportId = LeftId;
		const ImGuiID DetailsId = ImGui::DockBuilderSplitNode(ViewportId, ImGuiDir_Down, 0.45f, nullptr, &ViewportId);

		ImGui::DockBuilderDockWindow(ViewportPanel, ViewportId);
		ImGui::DockBuilderDockWindow(EmittersPanel, EmittersId);
		ImGui::DockBuilderDockWindow(DetailsPanel, DetailsId);
		ImGui::DockBuilderFinish(DockId);
	}
}

FParticleSystemEditor::FParticleSystemEditor(UParticleSystem* InSystem, int32 InEditorId)
	: FAssetEditorToolkit(InEditorId)
	, System(InSystem)
	, PreviewScene(MakeUnique<FParticleSystemPreviewScene>(InSystem))
	// ### 뒤가 ImGui 창 ID. 에셋 키로 두어 imgui.ini에 에셋별 마지막 위치 / 도킹이 저장되게 한다
	, WindowTitle(std::format("{} - Particle Editor###ParticleEditor_{}", GetDisplayName(InSystem->GetPath()), InSystem->GetPath()))
	, ViewportPanelTitle(std::format("Viewport###ParticleViewport_{}", InSystem->GetPath()))
	, EmittersPanelTitle(std::format("Emitters###ParticleEmitters_{}", InSystem->GetPath()))
	, DetailsPanelTitle(std::format("Details###ParticleDetails_{}", InSystem->GetPath()))
{
	// 기본 이미터가 위로 2m 정도 올라가므로 1m 높이를 4m 거리에서 본다
	Viewport.FocusOn(FVector(0.0f, 0.0f, 1.0f), 4.0f);
	Viewport.SetSceneRadius(PreviewScene->GetSceneRadius());
}

URenderAsset* FParticleSystemEditor::GetEditedAsset() const
{
	return System;
}

TUniquePtr<FAssetEditorToolkit> FParticleSystemEditor::CreateReplacementEditor(int32 NewEditorId) const
{
	// 키로 찾으면 retired는 건너뛰므로 새 객체가 나온다. 없으면 옛 객체를 계속 보여 준다
	UParticleSystem* NewSystem = UAssetManager::GetAssetByPath<UParticleSystem>(System->GetPath());
	if (!NewSystem)
	{
		return nullptr;
	}
	return MakeUnique<FParticleSystemEditor>(NewSystem, NewEditorId);
}

FString FParticleSystemEditor::GetViewportStats() const
{
	const UParticleSystemComponent* Component = PreviewScene->GetParticleComponent();
	return std::format("Emitters: {}\nParticles: {}", System->GetEmitters().Num(), Component->GetNumActiveParticles());
}

void FParticleSystemEditor::OnRender()
{
	if (bFocusRequested)
	{
		ImGui::SetNextWindowFocus();
		bFocusRequested = false;
	}

	ImGui::SetNextWindowSize(ImVec2(EditorUIScale::Px(1100.0f), EditorUIScale::Px(650.0f)), ImGuiCond_FirstUseEver);
	// 루트 창이라 Level Editor나 다른 에셋 에디터와 붙을 수 있고, 레벨 패널과는 섞이지 않는다
	ImGui::SetNextWindowClass(&FEditorUI::GetRootWindowClass());
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	const bool bHostVisible = ImGui::Begin(WindowTitle.c_str(), &bIsOpen,
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();
	bool bAnyPanelFocused = bHostVisible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	// 이 에셋의 패널끼리만 붙는 도킹 공간
	const ImGuiID PanelDockId = ImGui::GetID("PanelDockSpace");
	ImGuiWindowClass PanelClass;
	PanelClass.ClassId = PanelDockId;
	PanelClass.DockingAllowUnclassed = false;
	if (!ImGui::DockBuilderGetNode(PanelDockId))
	{
		BuildDefaultPanelLayout(PanelDockId, ImGui::GetWindowSize(), ViewportPanelTitle.c_str(), EmittersPanelTitle.c_str(), DetailsPanelTitle.c_str());
	}
	// 탭이 가려져 Begin이 false여도 호출해야 패널의 도킹이 풀리지 않는다
	ImGui::DockSpace(PanelDockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None, &PanelClass);
	ImGui::End();

	const auto BeginPanel = [&](const FString& Title, ImGuiWindowFlags Flags)
	{
		ImGui::SetNextWindowClass(&PanelClass);
		const bool bPanelVisible = ImGui::Begin(Title.c_str(), nullptr, Flags | ImGuiWindowFlags_NoCollapse);
		bAnyPanelFocused |= bPanelVisible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		return bPanelVisible;
	};

	// 휠은 줌에 쓰므로 스크롤을 끈다
	bIsVisible = BeginPanel(ViewportPanelTitle, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	if (bIsVisible)
	{
		DrawViewport();
	}
	ImGui::End();

	// TODO: 이미터 열 + 모듈 목록
	if (BeginPanel(EmittersPanelTitle, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImGui::TextDisabled("Emitters");
	}
	ImGui::End();

	// TODO: 선택한 이미터 / 모듈 속성
	if (BeginPanel(DetailsPanelTitle, ImGuiWindowFlags_None))
	{
		ImGui::TextDisabled("Details");
	}
	ImGui::End();

	bIsFocused = bAnyPanelFocused;
}
