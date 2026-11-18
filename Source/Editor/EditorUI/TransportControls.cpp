#include "EnginePCH.h"

#include "Editor/EditorUI/TransportControls.h"

#include "Render/Texture2D.h"

#include <imgui.h>

namespace TransportControls
{
	bool DrawButton(const char* Id, UTexture2D* Icon, const char* FallbackLabel, const char* Tooltip)
	{
		bool bClicked = false;
		if (Icon && Icon->GetResource())
		{
			const float IconSize = ImGui::GetTextLineHeight();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			bClicked = ImGui::ImageButton(Id, (ImTextureID)Icon->GetResource()->GetSRV(), ImVec2(IconSize, IconSize));
			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::PushID(Id);
			bClicked = ImGui::Button(FallbackLabel);
			ImGui::PopID();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", Tooltip);
		}
		return bClicked;
	}

	bool BeginSpeedMenu(const char* Label)
	{
		// 줄 오른쪽 끝에 붙인다. 폭은 가장 긴 글자 기준이라 값이 바뀌어도 그대로
		const float ButtonWidth = ImGui::CalcTextSize("x10.0").x + ImGui::GetStyle().FramePadding.x * 2.0f;
		const float RemainingWidth = ImGui::GetContentRegionAvail().x;
		if (RemainingWidth > ButtonWidth)
		{
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + RemainingWidth - ButtonWidth);
		}
		// 글자가 바뀌어도 ID가 같도록 ## 뒤에 고정 ID를 붙인다.
		// PushID로 감싸면 열린 팝업 창 안에서 PopID가 불려 ID 스택이 어긋나므로 쓰지 않는다
		const FString ButtonLabel = FString(Label) + "##PlaybackSpeed";
		if (ImGui::Button(ButtonLabel.c_str(), ImVec2(ButtonWidth, 0.0f)))
		{
			ImGui::OpenPopup("##PlaybackSpeedMenu");
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Playback Speed");
		}
		return ImGui::BeginPopup("##PlaybackSpeedMenu");
	}
}
