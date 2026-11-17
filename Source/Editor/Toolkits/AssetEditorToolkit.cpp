#include "EnginePCH.h"

#include "Editor/Toolkits/AssetEditorToolkit.h"

#include "Asset/RenderAsset.h"
#include "Editor/EditorUI/EditorUIScale.h"

#include <imgui.h>

FAssetEditorToolkit::FAssetEditorToolkit(int32 InEditorId)
	: EditorId(InEditorId)
{
}

FAssetEditorToolkit::~FAssetEditorToolkit() = default;

void FAssetEditorToolkit::Tick(float DeltaTime)
{
	// Tick이 OnRender보다 먼저 돌기 때문에 지난 프레임의 가시성으로 판단한다 (1프레임 지연은 무시 가능)
	if (!bIsVisible)
	{
		return;
	}

	GetPreviewScene()->Tick(DeltaTime);
	// 렌더가 Tick 뒤에 오므로 여기서 타깃과 카메라를 맞춰 둔다
	Viewport.Tick();
}

bool FAssetEditorToolkit::IsEditedAssetRetired() const
{
	return GetEditedAsset()->IsRetired();
}

UWorld* FAssetEditorToolkit::GetPreviewWorld() const
{
	return GetPreviewScene()->GetWorld();
}

void FAssetEditorToolkit::HandleViewportInput()
{
	Viewport.HandleCameraInput(true);
}

void FAssetEditorToolkit::DrawViewport()
{
	DrawViewportToolbar();

	// 남은 영역 크기를 기록해 두면 다음 Tick에서 타깃을 맞춘다 (1프레임 지연)
	const ImVec2 Avail = ImGui::GetContentRegionAvail();
	Viewport.SetDesiredSize(Avail.x > 1.0f ? static_cast<uint32>(Avail.x) : 0, Avail.y > 1.0f ? static_cast<uint32>(Avail.y) : 0);

	FTexture2D* ColorTarget = Viewport.GetColorTarget();
	if (!ColorTarget)
	{
		return;
	}

	const ImVec2 ImageSize(static_cast<float>(Viewport.GetTargetWidth()), static_cast<float>(Viewport.GetTargetHeight()));
	const ImVec2 ImagePos = ImGui::GetCursorScreenPos();
	ImGui::Image((ImTextureID)ColorTarget->GetSRV(), ImageSize);

	// 왼쪽 위 통계 (레벨 뷰포트 stat 표시처럼 이미지 위에 겹쳐 그린다)
	const FString Stats = GetViewportStats();
	if (!Stats.empty())
	{
		const ImVec2 StatsPos(ImagePos.x + EditorUIScale::Px(8.0f), ImagePos.y + EditorUIScale::Px(8.0f));
		const ImVec2 StatsSize = ImGui::CalcTextSize(Stats.c_str());
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(ImVec2(StatsPos.x - EditorUIScale::Px(4.0f), StatsPos.y - EditorUIScale::Px(4.0f)),
			ImVec2(StatsPos.x + StatsSize.x + EditorUIScale::Px(4.0f), StatsPos.y + StatsSize.y + EditorUIScale::Px(4.0f)), IM_COL32(0, 0, 0, 110), EditorUIScale::Px(3.0f));
		DrawList->AddText(StatsPos, IM_COL32(255, 255, 255, 230), Stats.c_str());
	}

	// Image는 입력을 받지 않으므로 같은 자리에 같은 크기의 투명 버튼을 올려 마우스 입력을 받는다
	ImGui::SetCursorScreenPos(ImagePos);
	ImGui::InvisibleButton("##PreviewViewportInput", ImageSize,
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	HandleViewportInput();
}

FString FAssetEditorToolkit::GetDisplayName(const FString& AssetKey)
{
	const size_t Slash = AssetKey.find_last_of("/\\");
	FString Name = Slash == FString::npos ? AssetKey : AssetKey.substr(Slash + 1);
	const size_t Dot = Name.find_last_of('.');
	if (Dot != FString::npos && Dot > 0)
	{
		Name.resize(Dot);
	}
	return Name;
}

void FAssetEditorToolkit::ApplyFocusRequest()
{
	if (FocusRequestFrames > 0)
	{
		ImGui::SetNextWindowFocus();
		--FocusRequestFrames;
	}
}
