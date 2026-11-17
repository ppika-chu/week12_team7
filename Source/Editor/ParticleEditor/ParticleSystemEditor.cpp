#include "EnginePCH.h"

#include "Editor/ParticleEditor/ParticleSystemEditor.h"
#include "Editor/EditorUI/EditorUI.h"
#include "Editor/EditorUI/EditorUIScale.h"
#include "Editor/EditorUI/TransportControls.h"
#include "Editor/Details/DetailsPanel.h"
#include "Editor/HitoriEd/EditorResourcePaths.h"

#include "Asset/AssetManager.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSystem.h"
#include "Render/LineBatcher.h"

#include <imgui.h>
#include <algorithm>
#include <format>
#include <iterator>
#include <unordered_map>

namespace
{
	// 이미터 열 폭 (px). UE FCascadeEmitterCanvasClient와 같다
	constexpr float EmitterWidth = 180.0f;

	// UE는 UCLASS DisplayName 메타데이터를 쓴다. 여기서는 표로 두고, 없으면 클래스 이름에서 접두어를 뗀다
	// TODO: 모듈이 추가될 때마다 이 표를 고쳐야 한다. UParticleModule에 virtual GetDisplayName()을 두고
	//       각 모듈이 이름을 돌려주게 바꾸면 표를 없앨 수 있다 (런타임 모듈 담당과 상의)
	FString GetModuleDisplayName(const UParticleModule* Module)
	{
		static const std::unordered_map<FString, FString> DisplayNames = {
			{ "UParticleModuleRequired", "Required" },
			{ "UParticleModuleSpawn", "Spawn" },
			{ "UParticleModuleLifetime", "Lifetime" },
			{ "UParticleModuleLocation", "Initial Location" },
			{ "UParticleModuleVelocity", "Initial Velocity" },
			{ "UParticleModuleSize", "Initial Size" },
			{ "UParticleModuleSizeMultiplyLife", "Size By Life" },
			{ "UParticleModuleSizeScaleBySpeed", "Size By Speed" },
			{ "UParticleModuleColor", "Initial Color" },
			{ "UParticleModuleColorOverLife", "Color Over Life" },
		};
		const FString& ClassName = Module->GetClass()->Name;
		const auto Found = DisplayNames.find(ClassName);
		if (Found != DisplayNames.end())
		{
			return Found->second;
		}
		const FString Prefix = "UParticleModule";
		return ClassName.rfind(Prefix, 0) == 0 ? ClassName.substr(Prefix.size()) : ClassName;
	}

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

void FParticleSystemEditor::DrawEmitterCanvas()
{
	const TArray<UParticleEmitter*>& Emitters = System->GetEmitters();
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float Width = EditorUIScale::Px(EmitterWidth);
	// 헤더 2줄(이름, 최대 파티클 수), 모듈 1줄. 체크박스 높이에 맞춘다
	const float RowHeight = ImGui::GetFrameHeight() + Style.ItemSpacing.y;
	const float HeadHeight = RowHeight * 2.0f;

	// 캔버스 크기: 열 수 + 빈 열 하나 (UE도 빈 곳에서 우클릭할 수 있게 한 칸 더 둔다)
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	const float CanvasWidth = std::max((Width + Style.ItemSpacing.x) * static_cast<float>(Emitters.Num() + 1), Avail.x);
	const float CanvasHeight = std::max(Avail.y, 1.0f);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	// 빈 곳 클릭 = 선택 해제. 위에 올라갈 블록이 입력을 받도록 겹침을 허용한다
	ImGui::SetNextItemAllowOverlap();
	if (ImGui::InvisibleButton("##EmitterCanvasBackground", ImVec2(CanvasWidth, CanvasHeight)))
	{
		SelectedEmitter = nullptr;
		SelectedModule = nullptr;
	}

	for (int32 EmitterIndex = 0; EmitterIndex < Emitters.Num(); ++EmitterIndex)
	{
		UParticleEmitter* Emitter = Emitters[EmitterIndex];
		// 지금은 LOD 0만 편집한다 (UE는 툴바에서 고른 LOD)
		UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(0) : nullptr;
		if (!LODLevel)
		{
			continue;
		}

		ImGui::PushID(EmitterIndex);
		const float X = Origin.x + (Width + Style.ItemSpacing.x) * static_cast<float>(EmitterIndex);
		const float Y = Origin.y;

		// 헤더: [켜기] 이름 / 최대 파티클 수
		ImGui::SetCursorScreenPos(ImVec2(X, Y));
		ImGui::SetNextItemAllowOverlap();
		if (ImGui::InvisibleButton("##Header", ImVec2(Width, HeadHeight)))
		{
			SelectedEmitter = Emitter;
			SelectedModule = nullptr;
		}
		const bool bEmitterSelected = Emitter == SelectedEmitter && SelectedModule == nullptr;
		// 꺼진 이미터는 헤더 전체(배경, 체크박스, 글자)를 흐리게
		const float HeaderAlpha = LODLevel->bEnabled ? 1.0f : Style.DisabledAlpha;
		DrawList->AddRectFilled(ImVec2(X, Y), ImVec2(X + Width, Y + HeadHeight),
			ImGui::GetColorU32(bEmitterSelected ? ImGuiCol_ButtonActive : (ImGui::IsItemHovered() ? ImGuiCol_HeaderHovered : ImGuiCol_Header), HeaderAlpha),
			Style.FrameRounding);

		const float TextOffsetY = (RowHeight - ImGui::GetTextLineHeight()) * 0.5f;
		ImGui::SetCursorScreenPos(ImVec2(X + Style.FramePadding.x, Y + Style.ItemSpacing.y * 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Style.Alpha * HeaderAlpha);
		if (ImGui::Checkbox("##EmitterEnabled", &LODLevel->bEnabled))
		{
			// UE FCascade::ToggleEnableOnSelectedEmitter 뒤 재시작. 레벨 재시작은 편집 반영 단계에서
			PreviewScene->RestartSimulation();
		}
		ImGui::PopStyleVar();
		const float NameX = X + Style.FramePadding.x + ImGui::GetFrameHeight() + Style.ItemSpacing.x;
		DrawList->AddText(ImVec2(NameX, Y + TextOffsetY), ImGui::GetColorU32(ImGuiCol_Text, HeaderAlpha), Emitter->EmitterName.c_str());
		// UE 헤더 숫자: 지금까지 잡은 최대 파티클 칸 수 (에셋에 저장된다)
		const FString PeakText = std::format("Peak: {}", LODLevel->PeakActiveParticles);
		DrawList->AddText(ImVec2(NameX, Y + RowHeight + TextOffsetY), ImGui::GetColorU32(ImGuiCol_TextDisabled, HeaderAlpha), PeakText.c_str());

		// 0번 줄 TypeData, 1번 Required, 2번 Spawn, 3번부터 일반 모듈 (TypeData가 없으면 0번 줄을 비우지 않고 당긴다)
		float ModuleY = Y + HeadHeight + Style.ItemSpacing.y;
		const auto DrawRow = [&](UParticleModule* Module, bool bDrawEnableButton)
		{
			DrawModuleBlock(Emitter, Module, X, ModuleY, RowHeight, bDrawEnableButton);
			ModuleY += RowHeight + 1.0f;
		};
		if (LODLevel->TypeDataModule)
		{
			DrawRow(LODLevel->TypeDataModule, false);
		}
		if (LODLevel->RequiredModule)
		{
			DrawRow(LODLevel->RequiredModule, false);
		}
		if (LODLevel->SpawnModule)
		{
			DrawRow(LODLevel->SpawnModule, true);
		}
		for (UParticleModule* Module : LODLevel->GetModules())
		{
			if (Module)
			{
				DrawRow(Module, true);
			}
		}
		ImGui::PopID();
	}

	// 가로 스크롤 범위를 캔버스 폭으로 잡는다
	ImGui::SetCursorScreenPos(Origin);
	ImGui::Dummy(ImVec2(CanvasWidth, 0.0f));
}

void FParticleSystemEditor::DrawModuleBlock(UParticleEmitter* Emitter, UParticleModule* Module, float X, float Y, float Height, bool bDrawEnableButton)
{
	const ImGuiStyle& Style = ImGui::GetStyle();
	const float Width = EditorUIScale::Px(EmitterWidth);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();

	ImGui::PushID(Module);
	ImGui::SetCursorScreenPos(ImVec2(X, Y));
	ImGui::SetNextItemAllowOverlap();
	if (ImGui::InvisibleButton("##Module", ImVec2(Width, Height)))
	{
		// UE와 같이 모듈을 고르면 그 이미터도 선택된다
		SelectedEmitter = Emitter;
		SelectedModule = Module;
	}

	// 평소 회색, 선택하면 엔진 강조색(분홍). 모듈이나 이미터가 꺼지면 줄 전체(배경, 체크박스, 글자)를 흐리게
	const bool bSelected = Module == SelectedModule;
	const UParticleLODLevel* LODLevel = Emitter->GetLODLevel(0);
	const float RowAlpha = (Module->bEnabled && LODLevel && LODLevel->bEnabled) ? 1.0f : Style.DisabledAlpha;
	const ImGuiCol ColorIndex = bSelected ? ImGuiCol_ButtonActive : (ImGui::IsItemHovered() ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg);
	DrawList->AddRectFilled(ImVec2(X, Y), ImVec2(X + Width, Y + Height), ImGui::GetColorU32(ColorIndex, RowAlpha), Style.FrameRounding);

	// [켜기] 이름. 체크박스가 없는 Required / TypeData도 이름 위치를 맞춘다
	const float CheckX = X + Style.FramePadding.x;
	const float CheckY = Y + (Height - ImGui::GetFrameHeight()) * 0.5f;
	if (bDrawEnableButton)
	{
		ImGui::SetCursorScreenPos(ImVec2(CheckX, CheckY));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, Style.Alpha * RowAlpha);
		if (ImGui::Checkbox("##ModuleEnabled", &Module->bEnabled))
		{
			// UE: bEnabled 토글 → PostEditChange → OnRestartInLevel. 실행 중에도 bEnabled를 보므로 목록은 다시 만들지 않는다
			PreviewScene->RestartSimulation();
		}
		ImGui::PopStyleVar();
	}
	const FString Name = GetModuleDisplayName(Module);
	const ImVec2 NamePos(CheckX + ImGui::GetFrameHeight() + Style.ItemSpacing.x, Y + (Height - ImGui::GetTextLineHeight()) * 0.5f);
	DrawList->AddText(NamePos, ImGui::GetColorU32(ImGuiCol_Text, RowAlpha), Name.c_str());
	ImGui::PopID();
}

void FParticleSystemEditor::DrawDetails()
{
	// UE와 같이 모듈을 골랐으면 모듈, 이미터만 골랐으면 이미터, 아무것도 안 골랐으면 파티클 시스템
	UObject* Target = System;
	FString Title = "Particle System";
	if (SelectedModule)
	{
		Target = SelectedModule;
		Title = GetModuleDisplayName(SelectedModule);
	}
	else if (SelectedEmitter)
	{
		Target = SelectedEmitter;
		Title = SelectedEmitter->EmitterName;
	}
	ImGui::TextUnformatted(Title.c_str());
	ImGui::Separator();

	// 편집할 수 있는 속성이 하나도 없으면 빈 화면 대신 안내를 띄운다
	bool bHasEditableProperty = false;
	for (const UClass* Class = Target->GetClass(); Class && !bHasEditableProperty; Class = Class->Super)
	{
		for (const FProperty& Property : Class->GetProperties())
		{
			bHasEditableProperty |= Property.HasAnyFlags(CPF_Edit);
		}
	}
	if (!bHasEditableProperty)
	{
		ImGui::TextDisabled("No editable properties");
		return;
	}

	// 이름 | 값 표. 값이 바뀌면 PostEditChangeProperty까지 부른다
	// TODO: Distribution 멤버(Lifetime, StartSize 등)는 리플렉션 등록 후 자동으로 보인다
	if (DrawObjectProperties(Target, nullptr, EPropertyLayout::Table))
	{
		// TODO: 편집 반영 단계에서 UE FCascade::ForceUpdate(UpdateAllModuleLists + ResetParticles + InitializeSystem)로 바꾼다
		PreviewScene->RestartSimulation();
	}
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

	if (BeginPanel(EmittersPanelTitle, ImGuiWindowFlags_HorizontalScrollbar))
	{
		DrawEmitterCanvas();
	}
	ImGui::End();

	if (BeginPanel(DetailsPanelTitle, ImGuiWindowFlags_None))
	{
		DrawDetails();
	}
	ImGui::End();

	bIsFocused = bAnyPanelFocused;
}
