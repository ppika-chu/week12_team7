#include "EnginePCH.h"

#include "Editor/ParticleEditor/ParticleSystemEditor.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorUIScale.h"
#include "Editor/EditorUI/TransportControls.h"
#include "Editor/HitoriEd/EditorResourcePaths.h"

#include "Asset/AssetManager.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Render/LineBatcher.h"

#include <imgui.h>
#include <algorithm>
#include <format>
#include <iterator>

namespace
{
	// Cascade Time 메뉴의 재생 속도 (100 / 50 / 25 / 10 / 1%)
	constexpr float SimulationSpeeds[] = { 1.0f, 0.5f, 0.25f, 0.1f, 0.01f };
	constexpr const char* SimulationSpeedLabels[] = { "x1.0", "x0.5", "x0.25", "x0.1", "x0.01" };

	// 켜진 토글은 눌린 버튼 색으로 그린다
	bool ToggleButton(const char* Label, bool& bValue)
	{
		if (bValue)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}
		const bool bClicked = ImGui::Button(Label);
		if (bValue)
		{
			ImGui::PopStyleColor();
		}
		if (bClicked)
		{
			bValue = !bValue;
		}
		return bClicked;
	}

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

	// Resources는 스캔하지 않으므로 경로로 불러온다. AssetMap에 캐시된다
	UAssetManager& AssetManager = UAssetManager::Get();
	PlayIcon = AssetManager.LoadTexture(EditorResourcePaths::AnimPlayForwardIcon);
	PauseIcon = AssetManager.LoadTexture(EditorResourcePaths::AnimPauseIcon);
	LoopingIcon = AssetManager.LoadTexture(EditorResourcePaths::AnimLoopingIcon);
	NoLoopingIcon = AssetManager.LoadTexture(EditorResourcePaths::AnimNoLoopingIcon);
}

void FParticleSystemEditor::Tick(float DeltaTime)
{
	FAssetEditorToolkit::Tick(DeltaTime);
	if (!bIsVisible)
	{
		return;
	}

	// UE FCascade::Tick과 같다. 끝난 걸 본 뒤 0.5초 기다렸다가 다시 재생한다
	constexpr double ResetInterval = 0.5;
	TotalTime += DeltaTime * PreviewScene->GetTimeScale();
	if (!bLoopSystem)
	{
		return;
	}
	if (bPendingReset)
	{
		if (TotalTime > ResetTime)
		{
			PreviewScene->RestartSimulation();
			bPendingReset = false;
		}
	}
	else if (PreviewScene->GetParticleComponent()->HasCompleted())
	{
		bPendingReset = true;
		ResetTime = TotalTime + ResetInterval;
	}
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

void FParticleSystemEditor::DrawPreviewOverlay(FLineBatcher& LineBatcher) const
{
	if (bShowBounds)
	{
		// 컬링에 쓰는 것과 같은 월드 AABB
		LineBatcher.AddBox(PreviewScene->GetParticleComponent()->CalcBounds(), FVector4(1.0f, 1.0f, 0.0f, 1.0f));
	}
}

void FParticleSystemEditor::DrawToolbar()
{
	// 창 패딩이 0이라 직접 띄운다. 도킹 탭 아래부터 시작하도록 지금 커서에 더한다
	const ImGuiStyle& Style = ImGui::GetStyle();
	const ImVec2 Cursor = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(Cursor.x + Style.ItemSpacing.x, Cursor.y + Style.ItemSpacing.y));
	if (ImGui::Button("Restart Sim"))
	{
		PreviewScene->RestartSimulation();
	}
	ImGui::SameLine();
	ToggleButton("Bounds", bShowBounds);
	ImGui::SameLine();
	ToggleButton("Origin Axis", bShowOriginAxis);
	ImGui::Dummy(ImVec2(0.0f, Style.ItemSpacing.y * 0.5f));

	// 원점 축은 뷰포트 기즈모(이동 모드)로 그린다. 입력은 받지 않으므로 그리기만 한다
	FGizmo& Gizmo = Viewport.GetGizmo();
	if (bShowOriginAxis)
	{
		Gizmo.SetMode(EGizmoMode::Location);
		Gizmo.SetPivot(FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f));
	}
	else
	{
		Gizmo.ClearPivot();
	}
}

void FParticleSystemEditor::DrawViewportToolbar()
{
	if (ImGui::Button("View"))
	{
		ImGui::OpenPopup("##ParticleViewMenu");
	}
	if (ImGui::BeginPopup("##ParticleViewMenu"))
	{
		ImGui::MenuItem("Particle Counts", nullptr, &bShowParticleCounts);
		// Cascade의 Geometry. 바닥 메시만 켜고 끈다
		if (ImGui::MenuItem("Geometry", nullptr, PreviewScene->IsFloorVisible()))
		{
			PreviewScene->SetFloorVisible(!PreviewScene->IsFloorVisible());
		}
		ImGui::EndPopup();
	}

	// 키보드 내비게이션을 끈다. 켜 두면 누른 버튼이 포커스를 받아 스페이스가 그 버튼도 누른다
	ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
	ImGui::SameLine();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(EditorUIScale::Px(2.0f), ImGui::GetStyle().ItemSpacing.y));
	const bool bPaused = PreviewScene->IsPaused();
	if (TransportControls::DrawButton("##PlayPause", bPaused ? PlayIcon : PauseIcon, bPaused ? ">" : "||", bPaused ? "Play" : "Pause"))
	{
		PreviewScene->SetPaused(!bPaused);
	}
	ImGui::SameLine();
	if (TransportControls::DrawButton("##Loop", bLoopSystem ? LoopingIcon : NoLoopingIcon, bLoopSystem ? "Loop" : "No Loop", bLoopSystem ? "Looping" : "No Looping"))
	{
		// UE FCascade::OnToggleLoopSystem. 끄면 기다리던 재시작도 취소한다
		bLoopSystem = !bLoopSystem;
		bPendingReset = bLoopSystem && bPendingReset;
	}
	ImGui::PopStyleVar();

	ImGui::SameLine();
	int32 CurrentIndex = 0;
	for (int32 i = 0; i < static_cast<int32>(std::size(SimulationSpeeds)); ++i)
	{
		CurrentIndex = PreviewScene->GetSimulationSpeed() == SimulationSpeeds[i] ? i : CurrentIndex;
	}
	if (TransportControls::BeginSpeedMenu(SimulationSpeedLabels[CurrentIndex]))
	{
		for (int32 i = 0; i < static_cast<int32>(std::size(SimulationSpeeds)); ++i)
		{
			if (ImGui::MenuItem(SimulationSpeedLabels[i], nullptr, i == CurrentIndex))
			{
				PreviewScene->SetSimulationSpeed(SimulationSpeeds[i]);
			}
		}
		ImGui::EndPopup();
	}
	ImGui::PopItemFlag();
}

FString FParticleSystemEditor::GetViewportStats() const
{
	if (!bShowParticleCounts)
	{
		return FString();
	}
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

	DrawToolbar();

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
