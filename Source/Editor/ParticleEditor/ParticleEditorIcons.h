#pragma once

#include <imgui.h>
#include <algorithm>
#include <cmath>

// 파티클 에디터 툴바 아이콘. 이미지 파일 없이 ImDrawList로 단색 선 아이콘을 그린다.
// 색은 ImGui 글자색을 따르므로 BeginDisabled 안에서는 자동으로 흐려진다
namespace ParticleEditorIcons
{
	// 아이콘을 그릴 사각형(버튼 안쪽 여백을 뺀 영역)과 선 색
	using FDrawIcon = void (*)(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color);

	inline float GetThickness(const ImVec2& Min, const ImVec2& Max)
	{
		return std::max(1.5f, (Max.x - Min.x) * 0.1f);
	}

	// 정사각 버튼에 아이콘을 그린다. bActive면 눌린 버튼 색 (켜진 토글, 지금 보간 방식)
	inline bool IconButton(const char* Id, FDrawIcon Draw, const char* Tooltip, bool bActive = false)
	{
		const float Size = ImGui::GetFrameHeight();
		if (bActive)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}
		const bool bClicked = ImGui::Button(Id, ImVec2(Size, Size));
		if (bActive)
		{
			ImGui::PopStyleColor();
		}
		const ImVec2 ItemMin = ImGui::GetItemRectMin();
		const ImVec2 ItemMax = ImGui::GetItemRectMax();
		const float Pad = Size * 0.24f;
		Draw(ImGui::GetWindowDrawList(), ImVec2(ItemMin.x + Pad, ItemMin.y + Pad), ImVec2(ItemMax.x - Pad, ItemMax.y - Pad), ImGui::GetColorU32(ImGuiCol_Text));
		if (Tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", Tooltip);
		}
		return bClicked;
	}

	// ---- 상단 툴바 ----

	// 디스켓
	inline void DrawSave(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;
		DrawList->AddRect(Min, Max, Color, W * 0.12f, 0, T);
		DrawList->AddRect(ImVec2(Min.x + W * 0.28f, Min.y), ImVec2(Max.x - W * 0.28f, Min.y + H * 0.34f), Color, 0.0f, 0, T);
		DrawList->AddRectFilled(ImVec2(Min.x + W * 0.24f, Min.y + H * 0.58f), ImVec2(Max.x - W * 0.24f, Max.y - H * 0.12f), Color);
	}

	// 끝에 화살촉이 달린 원 (다시 시작)
	inline void DrawRestart(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
		const float Radius = (Max.x - Min.x) * 0.42f;
		const float Pi = 3.14159265f;
		const float StartAngle = -0.35f * Pi;
		const float EndAngle = 1.25f * Pi;
		DrawList->PathArcTo(Center, Radius, StartAngle, EndAngle, 24);
		DrawList->PathStroke(Color, ImDrawFlags_None, T);
		// 화살촉: 호의 끝에서 진행 방향으로
		const ImVec2 End(Center.x + std::cos(EndAngle) * Radius, Center.y + std::sin(EndAngle) * Radius);
		const ImVec2 Dir(-std::sin(EndAngle), std::cos(EndAngle));
		const ImVec2 Out(std::cos(EndAngle), std::sin(EndAngle));
		const float S = Radius * 0.55f;
		DrawList->AddTriangleFilled(ImVec2(End.x + Dir.x * S, End.y + Dir.y * S), ImVec2(End.x + Out.x * S * 0.7f, End.y + Out.y * S * 0.7f),
			ImVec2(End.x - Out.x * S * 0.7f, End.y - Out.y * S * 0.7f), Color);
	}

	// 선만 있는 상자 (바운드)
	inline void DrawBounds(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max) * 0.8f;
		const float D = (Max.x - Min.x) * 0.3f;
		const ImVec2 F0(Min.x, Min.y + D), F1(Max.x - D, Max.y);
		const ImVec2 B0(Min.x + D, Min.y), B1(Max.x, Max.y - D);
		DrawList->AddRect(F0, F1, Color, 0.0f, 0, T);
		DrawList->AddRect(B0, B1, Color, 0.0f, 0, T);
		DrawList->AddLine(F0, B0, Color, T);
		DrawList->AddLine(ImVec2(F1.x, F0.y), ImVec2(B1.x, B0.y), Color, T);
		DrawList->AddLine(ImVec2(F0.x, F1.y), ImVec2(B0.x, B1.y), Color, T);
		DrawList->AddLine(F1, B1, Color, T);
	}

	// 원점에서 뻗은 세 축
	inline void DrawOriginAxis(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;
		const ImVec2 Origin(Min.x + W * 0.38f, Max.y - H * 0.38f);
		DrawList->AddLine(Origin, ImVec2(Origin.x, Min.y), Color, T);
		DrawList->AddLine(Origin, ImVec2(Max.x, Origin.y), Color, T);
		DrawList->AddLine(Origin, ImVec2(Min.x, Max.y), Color, T);
		DrawList->AddCircleFilled(Origin, T * 1.3f, Color);
	}

	// 카메라 (Save Thumbnail)
	inline void DrawThumbnail(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;
		DrawList->AddRectFilled(ImVec2(Min.x + W * 0.3f, Min.y + H * 0.1f), ImVec2(Max.x - W * 0.3f, Min.y + H * 0.3f), Color);
		DrawList->AddRect(ImVec2(Min.x, Min.y + H * 0.25f), ImVec2(Max.x, Max.y - H * 0.05f), Color, W * 0.12f, 0, T);
		DrawList->AddCircle(ImVec2(Min.x + W * 0.5f, Min.y + H * 0.6f), W * 0.2f, Color, 0, T);
	}

	// ---- LOD (UE Cascade 툴바의 LOD 버튼들) ----

	// 가운데 세로로 놓인 삼각형. bLeft면 왼쪽(더 높은 LOD)을 가리킨다
	inline void DrawArrowTriangle(ImDrawList* DrawList, float CenterX, const ImVec2& Min, const ImVec2& Max, bool bLeft, ImU32 Color)
	{
		const float H = Max.y - Min.y;
		const float HalfW = H * 0.3f;
		const float Tip = bLeft ? CenterX - HalfW : CenterX + HalfW;
		const float Back = bLeft ? CenterX + HalfW : CenterX - HalfW;
		DrawList->AddTriangleFilled(ImVec2(Tip, (Min.y + Max.y) * 0.5f), ImVec2(Back, Min.y + H * 0.12f), ImVec2(Back, Max.y - H * 0.12f), Color);
	}

	// |◀ 가장 높은 LOD
	inline void DrawLODHighest(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float W = Max.x - Min.x;
		DrawList->AddLine(ImVec2(Min.x + W * 0.12f, Min.y + W * 0.12f), ImVec2(Min.x + W * 0.12f, Max.y - W * 0.12f), Color, GetThickness(Min, Max));
		DrawArrowTriangle(DrawList, Min.x + W * 0.58f, Min, Max, true, Color);
	}

	// ◀ 한 단계 높은 LOD
	inline void DrawLODHigher(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		DrawArrowTriangle(DrawList, (Min.x + Max.x) * 0.5f, Min, Max, true, Color);
	}

	// ▶ 한 단계 낮은 LOD
	inline void DrawLODLower(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		DrawArrowTriangle(DrawList, (Min.x + Max.x) * 0.5f, Min, Max, false, Color);
	}

	// ▶| 가장 낮은 LOD
	inline void DrawLODLowest(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float W = Max.x - Min.x;
		DrawArrowTriangle(DrawList, Min.x + W * 0.42f, Min, Max, false, Color);
		DrawList->AddLine(ImVec2(Max.x - W * 0.12f, Min.y + W * 0.12f), ImVec2(Max.x - W * 0.12f, Max.y - W * 0.12f), Color, GetThickness(Min, Max));
	}

	// + 와 넣을 쪽을 가리키는 작은 삼각형
	inline void DrawLODAdd(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, bool bBefore, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float W = Max.x - Min.x;
		const float PlusX = bBefore ? Max.x - W * 0.3f : Min.x + W * 0.3f;
		const float CenterY = (Min.y + Max.y) * 0.5f;
		const float R = W * 0.28f;
		DrawList->AddLine(ImVec2(PlusX - R, CenterY), ImVec2(PlusX + R, CenterY), Color, T);
		DrawList->AddLine(ImVec2(PlusX, CenterY - R), ImVec2(PlusX, CenterY + R), Color, T);
		const ImVec2 SmallMin(Min.x, CenterY - W * 0.28f);
		const ImVec2 SmallMax(Max.x, CenterY + W * 0.28f);
		DrawArrowTriangle(DrawList, bBefore ? Min.x + W * 0.18f : Max.x - W * 0.18f, SmallMin, SmallMax, bBefore, Color);
	}

	inline void DrawLODAddBefore(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		DrawLODAdd(DrawList, Min, Max, true, Color);
	}

	inline void DrawLODAddAfter(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		DrawLODAdd(DrawList, Min, Max, false, Color);
	}

	// 휴지통
	inline void DrawDelete(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max) * 0.85f;
		const float W = Max.x - Min.x;
		const float H = Max.y - Min.y;
		DrawList->AddLine(ImVec2(Min.x, Min.y + H * 0.2f), ImVec2(Max.x, Min.y + H * 0.2f), Color, T);
		DrawList->AddLine(ImVec2(Min.x + W * 0.36f, Min.y), ImVec2(Max.x - W * 0.36f, Min.y), Color, T);
		const ImVec2 Body[] = { ImVec2(Min.x + W * 0.14f, Min.y + H * 0.2f), ImVec2(Min.x + W * 0.22f, Max.y), ImVec2(Max.x - W * 0.22f, Max.y), ImVec2(Max.x - W * 0.14f, Min.y + H * 0.2f) };
		DrawList->AddPolyline(Body, 4, Color, ImDrawFlags_None, T);
		DrawList->AddLine(ImVec2(Min.x + W * 0.5f, Min.y + H * 0.38f), ImVec2(Min.x + W * 0.5f, Max.y - H * 0.16f), Color, T * 0.8f);
	}

	// ---- 커브 에디터 툴바 ----

	// 네 귀퉁이 꺾쇠 (화면에 맞추기)
	inline void DrawFit(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float L = (Max.x - Min.x) * 0.32f;
		const ImVec2 TopLeft[] = { ImVec2(Min.x, Min.y + L), Min, ImVec2(Min.x + L, Min.y) };
		const ImVec2 TopRight[] = { ImVec2(Max.x - L, Min.y), ImVec2(Max.x, Min.y), ImVec2(Max.x, Min.y + L) };
		const ImVec2 BottomRight[] = { ImVec2(Max.x, Max.y - L), Max, ImVec2(Max.x - L, Max.y) };
		const ImVec2 BottomLeft[] = { ImVec2(Min.x + L, Max.y), ImVec2(Min.x, Max.y), ImVec2(Min.x, Max.y - L) };
		DrawList->AddPolyline(TopLeft, 3, Color, ImDrawFlags_None, T);
		DrawList->AddPolyline(TopRight, 3, Color, ImDrawFlags_None, T);
		DrawList->AddPolyline(BottomRight, 3, Color, ImDrawFlags_None, T);
		DrawList->AddPolyline(BottomLeft, 3, Color, ImDrawFlags_None, T);
	}

	// 부드러운 S 곡선. 모듈의 커브 버튼에도 쓴다
	inline void DrawCurve(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float W = Max.x - Min.x;
		DrawList->AddBezierCubic(ImVec2(Min.x, Max.y), ImVec2(Min.x + W * 0.5f, Max.y), ImVec2(Max.x - W * 0.5f, Min.y), ImVec2(Max.x, Min.y), Color, GetThickness(Min, Max));
	}

	inline void DrawKey(ImDrawList* DrawList, const ImVec2& At, float T, ImU32 Color)
	{
		DrawList->AddCircleFilled(At, T * 1.5f, Color);
	}

	// Auto: 곡선 + 가운데 키
	inline void DrawInterpAuto(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		DrawCurve(DrawList, Min, Max, Color);
		DrawKey(DrawList, ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f), GetThickness(Min, Max), Color);
	}

	// User: 곡선 + 키 + 기울기 손잡이
	inline void DrawInterpUser(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
		const float W = Max.x - Min.x;
		DrawCurve(DrawList, Min, Max, Color);
		const ImVec2 A(Center.x - W * 0.42f, Center.y + W * 0.12f);
		const ImVec2 B(Center.x + W * 0.42f, Center.y - W * 0.12f);
		DrawList->AddLine(A, B, Color, T * 0.7f);
		DrawList->AddCircle(A, T * 1.1f, Color, 0, T * 0.7f);
		DrawList->AddCircle(B, T * 1.1f, Color, 0, T * 0.7f);
		DrawKey(DrawList, Center, T, Color);
	}

	// Linear: 꺾은선 + 키
	inline void DrawInterpLinear(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const ImVec2 Mid((Min.x + Max.x) * 0.5f, Min.y + (Max.y - Min.y) * 0.15f);
		const ImVec2 Points[] = { ImVec2(Min.x, Max.y), Mid, ImVec2(Max.x, Max.y - (Max.y - Min.y) * 0.25f) };
		DrawList->AddPolyline(Points, 3, Color, ImDrawFlags_None, T);
		DrawKey(DrawList, Mid, T, Color);
	}

	// Constant: 계단 + 키
	inline void DrawInterpConstant(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float MidX = (Min.x + Max.x) * 0.5f;
		const float Low = Max.y - (Max.y - Min.y) * 0.1f;
		const float High = Min.y + (Max.y - Min.y) * 0.1f;
		const ImVec2 Points[] = { ImVec2(Min.x, Low), ImVec2(MidX, Low), ImVec2(MidX, High), ImVec2(Max.x, High) };
		DrawList->AddPolyline(Points, 4, Color, ImDrawFlags_None, T);
		DrawKey(DrawList, ImVec2(Min.x, Low), T, Color);
		DrawKey(DrawList, ImVec2(MidX, High), T, Color);
	}

	// Flatten: 꼭대기 키의 기울기를 수평으로
	inline void DrawFlatten(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, ImU32 Color)
	{
		const float T = GetThickness(Min, Max);
		const float W = Max.x - Min.x;
		const ImVec2 Top((Min.x + Max.x) * 0.5f, Min.y + (Max.y - Min.y) * 0.3f);
		DrawList->AddBezierCubic(ImVec2(Min.x, Max.y), ImVec2(Min.x + W * 0.2f, Top.y), ImVec2(Top.x - W * 0.25f, Top.y), Top, Color, T);
		DrawList->AddBezierCubic(Top, ImVec2(Top.x + W * 0.25f, Top.y), ImVec2(Max.x - W * 0.2f, Top.y), ImVec2(Max.x, Max.y), Color, T);
		// 꼭대기 키를 지나는 수평 손잡이
		DrawList->AddLine(ImVec2(Top.x - W * 0.45f, Top.y), ImVec2(Top.x + W * 0.45f, Top.y), Color, T * 0.7f);
		DrawKey(DrawList, Top, T, Color);
	}
}
