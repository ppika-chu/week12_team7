#pragma once

class UTexture2D;

// 애니메이션 / 파티클 에디터가 같이 쓰는 재생 버튼 (언리얼 SAnimTimelineTransportControls 모양)
namespace TransportControls
{
	// 배경 없는 아이콘 버튼 (언리얼 Animation.PlayControlsButton). 아이콘이 없으면 글자 버튼
	bool DrawButton(const char* Id, UTexture2D* Icon, const char* FallbackLabel, const char* Tooltip);

	// 줄 오른쪽 끝에 속도 버튼("x1.0")을 그리고, 눌러서 열린 메뉴를 시작한다. true면 항목을 그리고 ImGui::EndPopup
	bool BeginSpeedMenu(const char* Label);
}
