#include "EnginePCH.h"

#include "Editor/ParticleEditor/ParticleCurveEditor.h"
#include "Editor/EditorUI/EditorUIScale.h"

#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSystem.h"

#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <format>
#include <iterator>

namespace
{
	using CurveEd::IsCurveDistribution;
	using CurveEd::GetInterpModeName;
	using CurveEd::MakeTracks;

	// 모듈에 등록된 분포 프로퍼티(PROPERTY_DISTRIBUTION)를 돈다
	template<typename F>
	void ForEachDistributionProperty(UParticleModule* Module, F&& Visit)
	{
		for (const UClass* Class = Module->GetClass(); Class; Class = Class->Super)
		{
			for (const FProperty& Property : Class->GetProperties())
			{
				if (Property.Type == EPropertyType::Object && Property.Class && Property.Class->IsChildOf(UDistribution::StaticClass()))
				{
					UDistribution* Distribution = *reinterpret_cast<UDistribution**>(reinterpret_cast<char*>(Module) + Property.Offset);
					Visit(Property, Distribution);
				}
			}
		}
	}

	// 격자 간격: 1, 2, 5 x 10^n 중 화면에서 MinPixels 이상 벌어지는 가장 작은 값
	float GetGridStep(float Range, float Pixels, float MinPixels)
	{
		const float Raw = Range * MinPixels / std::max(Pixels, 1.0f);
		const float Magnitude = std::pow(10.0f, std::floor(std::log10(std::max(Raw, 1.0e-6f))));
		const float Normalized = Raw / Magnitude;
		return (Normalized <= 1.0f ? 1.0f : (Normalized <= 2.0f ? 2.0f : (Normalized <= 5.0f ? 5.0f : 10.0f))) * Magnitude;
	}

	// 커브 목록 색 (UE 커브 에디터처럼 커브마다 다른 색)
	constexpr uint32 EntryPalette[] = {
		IM_COL32(255, 170, 60, 255), IM_COL32(80, 210, 230, 255), IM_COL32(240, 220, 80, 255),
		IM_COL32(230, 110, 220, 255), IM_COL32(150, 230, 90, 255), IM_COL32(170, 150, 255, 255),
	};
	// 벡터 채널 색 (UE: X 빨강, Y 초록, Z 파랑)
	constexpr uint32 ChannelColors[] = { IM_COL32(235, 80, 80, 255), IM_COL32(90, 210, 90, 255), IM_COL32(90, 140, 245, 255) };

	const char* GetInterpModeName(uint8 Mode)
	{
		switch (static_cast<EInterpCurveMode>(Mode))
		{
		case EInterpCurveMode::Linear: return "Linear";
		case EInterpCurveMode::Constant: return "Constant";
		case EInterpCurveMode::CurveAuto: return "Auto";
		case EInterpCurveMode::CurveUser: return "User";
		}
		return "";
	}
}

bool FParticleCurveEditor::HasCurves(UParticleModule* Module)
{
	bool bHasCurve = false;
	ForEachDistributionProperty(Module, [&](const FProperty&, UDistribution* Distribution) { bHasCurve |= IsCurveDistribution(Distribution); });
	return bHasCurve;
}

bool FParticleCurveEditor::HasModule(const UParticleModule* Module) const
{
	return std::any_of(Entries.begin(), Entries.end(), [Module](const FEntry& Entry) { return Entry.Module == Module; });
}

void FParticleCurveEditor::ToggleModule(UParticleModule* Module, const FString& ModuleName)
{
	Selection.clear();
	DragMode = EDragMode::None;
	if (HasModule(Module))
	{
		std::erase_if(Entries, [Module](const FEntry& Entry) { return Entry.Module == Module; });
		return;
	}
	ForEachDistributionProperty(Module, [&](const FProperty& Property, UDistribution* Distribution)
	{
		FEntry Entry;
		MakeTracks(Distribution, Entry.Tracks, Entry.TrackNames);
		if (Entry.Tracks.empty())
		{
			return;
		}
		Entry.Module = Module;
		Entry.Distribution = Distribution;
		Entry.Name = ModuleName + ": " + Property.Name;
		Entry.Color = EntryPalette[Entries.size() % std::size(EntryPalette)];
		Entries.push_back(std::move(Entry));
	});
	bFitRequested = true;
}

void FParticleCurveEditor::Clear()
{
	Entries.clear();
	Selection.clear();
	DragMode = EDragMode::None;
}

void FParticleCurveEditor::RemoveStaleEntries()
{
	const size_t OldCount = Entries.size();
	std::erase_if(Entries, [this](const FEntry& Entry)
	{
		// 모듈이 아직 시스템에 있는지 포인터 비교로만 확인한 뒤에 읽는다
		bool bModuleAlive = false;
		for (UParticleEmitter* Emitter : System->Emitters)
		{
			if (!Emitter)
			{
				continue;
			}
			for (UParticleLODLevel* LODLevel : Emitter->LODLevels)
			{
				if (!LODLevel)
				{
					continue;
				}
				bModuleAlive |= LODLevel->RequiredModule == Entry.Module || LODLevel->SpawnModule == Entry.Module || LODLevel->TypeDataModule == Entry.Module
					|| LODLevel->Modules.Find(Entry.Module) >= 0;
			}
		}
		if (!bModuleAlive)
		{
			return true;
		}
		// Details에서 분포 종류를 바꾸면 모듈은 새 분포를 가리키고 옛 분포는 지워졌다
		bool bDistributionAlive = false;
		ForEachDistributionProperty(Entry.Module, [&](const FProperty&, UDistribution* Distribution) { bDistributionAlive |= Distribution == Entry.Distribution; });
		return !bDistributionAlive;
	});
	if (Entries.size() != OldCount)
	{
		Selection.clear();
		DragMode = EDragMode::None;
	}
}

void FParticleCurveEditor::Draw()
{
	RemoveStaleEntries();
	DrawToolbar();
	const float ListWidth = EditorUIScale::Px(260.0f);
	DrawCurveList(ListWidth);
	ImGui::SameLine();
	DrawGraph();
}

void FParticleCurveEditor::DrawToolbar()
{
	if (ImGui::Button("Fit"))
	{
		FitView();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Fit all visible curves (F)");
	}

	// UE 커브 에디터 툴바: 선택한 키의 보간 방식
	ImGui::SameLine();
	ImGui::BeginDisabled(Selection.empty());
	const struct { const char* Label; EInterpCurveMode Mode; } Modes[] = {
		{ "Auto", EInterpCurveMode::CurveAuto }, { "User", EInterpCurveMode::CurveUser },
		{ "Linear", EInterpCurveMode::Linear }, { "Constant", EInterpCurveMode::Constant },
	};
	for (const auto& Mode : Modes)
	{
		if (ImGui::Button(Mode.Label))
		{
			SetSelectedInterpMode(static_cast<uint8>(Mode.Mode));
		}
		ImGui::SameLine();
	}
	if (ImGui::Button("Flatten"))
	{
		FlattenSelectedTangents();
	}
	ImGui::EndDisabled();

	// 키 하나만 골랐으면 시간과 값을 숫자로 고친다 (UE 키 우클릭 Set Time / Set Value)
	if (Selection.size() == 1)
	{
		const FKeyRef& Ref = Selection[0];
		FEntry& Entry = Entries[Ref.Entry];
		ICurveEdTrack& Track = *Entry.Tracks[Ref.Track];
		float Time = Track.GetKeyIn(Ref.Key);
		float Value = Track.GetKeyOut(Ref.Channel, Ref.Key);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(EditorUIScale::Px(80.0f));
		if (ImGui::DragFloat("Time", &Time, 0.01f))
		{
			Track.SetKeyIn(Ref.Key, Time);
			Track.AutoSetTangents();
			NotifyChanged(Entry);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(EditorUIScale::Px(80.0f));
		if (ImGui::DragFloat("Value", &Value, 0.01f))
		{
			Track.SetKeyOut(Ref.Channel, Ref.Key, Value);
			Track.AutoSetTangents();
			NotifyChanged(Entry);
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", GetInterpModeName(Track.GetKeyInterpMode(Ref.Key)));
	}
}

void FParticleCurveEditor::DrawCurveList(float Width)
{
	// 오른쪽 경계를 끌어 폭을 바꿀 수 있다. 긴 이름은 줄을 바꿔 다 보이게 한다
	ImGui::BeginChild("##CurveList", ImVec2(Width, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
	if (Entries.empty())
	{
		ImGui::TextWrapped("Click the curve button on a module to show its curves here.");
	}
	int32 RemoveIndex = -1;
	for (int32 EntryIndex = 0; EntryIndex < static_cast<int32>(Entries.size()); ++EntryIndex)
	{
		FEntry& Entry = Entries[EntryIndex];
		ImGui::PushID(EntryIndex);
		ImGui::Checkbox("##Visible", &Entry.bVisible);
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, Entry.Color);
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(Entry.Name.c_str());
		ImGui::PopTextWrapPos();
		ImGui::PopStyleColor();
		if (ImGui::BeginPopupContextItem("##EntryMenu"))
		{
			if (ImGui::MenuItem("Remove"))
			{
				RemoveIndex = EntryIndex;
			}
			ImGui::EndPopup();
		}

		// 채널 켜고 끄기. 색이 그래프의 선 색이다
		const int32 NumChannels = Entry.Tracks[0]->GetNumChannels();
		if (NumChannels > 1)
		{
			const char* Labels3[] = { "X", "Y", "Z" };
			const char* Labels2[] = { "Min", "Max" };
			ImGui::Indent();
			for (int32 Channel = 0; Channel < NumChannels; ++Channel)
			{
				ImGui::PushID(Channel);
				if (Channel > 0)
				{
					ImGui::SameLine();
				}
				ImGui::PushStyleColor(ImGuiCol_CheckMark, GetChannelColor(Entry, 0, Channel));
				ImGui::Checkbox(NumChannels == 2 ? Labels2[Channel] : Labels3[Channel], &Entry.bChannelVisible[Channel]);
				ImGui::PopStyleColor();
				ImGui::PopID();
			}
			ImGui::Unindent();
		}
		ImGui::PopID();
	}
	if (RemoveIndex >= 0)
	{
		Entries.erase(Entries.begin() + RemoveIndex);
		Selection.clear();
		DragMode = EDragMode::None;
	}
	ImGui::EndChild();
}

bool FParticleCurveEditor::IsChannelVisible(const FEntry& Entry, int32, int32 Channel) const
{
	return Entry.bVisible && Entry.bChannelVisible[Channel];
}

uint32 FParticleCurveEditor::GetChannelColor(const FEntry& Entry, int32 Track, int32 Channel) const
{
	const int32 NumChannels = Entry.Tracks[Track]->GetNumChannels();
	ImU32 Color = NumChannels == 3 ? ChannelColors[Channel] : Entry.Color;
	// Uniform의 최소 쪽(채널 0 또는 Min 트랙)은 흐리게
	const bool bMinSide = (NumChannels == 2 && Channel == 0) || (Entry.Tracks.size() == 2 && Track == 0);
	if (bMinSide)
	{
		Color = (Color & 0x00FFFFFF) | (150u << 24);
	}
	return Color;
}

bool FParticleCurveEditor::IsSelected(const FKeyRef& Ref) const
{
	return std::find(Selection.begin(), Selection.end(), Ref) != Selection.end();
}

void FParticleCurveEditor::NotifyChanged(FEntry& Entry)
{
	// 분포의 캐시 표시를 올린다. 커브는 매번 계산되지만 값이 바뀐 사실은 남긴다
	Entry.Distribution->MarkDirty();
	if (OnChanged)
	{
		OnChanged();
	}
}

void FParticleCurveEditor::FitView()
{
	float MinX = FLT_MAX, MaxX = -FLT_MAX, MinY = FLT_MAX, MaxY = -FLT_MAX;
	for (const FEntry& Entry : Entries)
	{
		for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Entry.Tracks.size()); ++TrackIndex)
		{
			const ICurveEdTrack& Track = *Entry.Tracks[TrackIndex];
			for (int32 Key = 0; Key < Track.GetNumKeys(); ++Key)
			{
				MinX = std::min(MinX, Track.GetKeyIn(Key));
				MaxX = std::max(MaxX, Track.GetKeyIn(Key));
			}
		}
	}
	if (MinX > MaxX)
	{
		MinX = 0.0f;
		MaxX = 1.0f;
	}
	if (MaxX - MinX < 1.0e-3f)
	{
		MinX -= 0.5f;
		MaxX += 0.5f;
	}
	// 값은 키 사이 곡선의 볼록한 부분까지 들어오게 표본으로 잰다
	for (const FEntry& Entry : Entries)
	{
		for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Entry.Tracks.size()); ++TrackIndex)
		{
			const ICurveEdTrack& Track = *Entry.Tracks[TrackIndex];
			for (int32 Channel = 0; Channel < Track.GetNumChannels(); ++Channel)
			{
				if (!IsChannelVisible(Entry, TrackIndex, Channel))
				{
					continue;
				}
				for (int32 Sample = 0; Sample <= 64; ++Sample)
				{
					const float Value = Track.Eval(Channel, MinX + (MaxX - MinX) * static_cast<float>(Sample) / 64.0f);
					MinY = std::min(MinY, Value);
					MaxY = std::max(MaxY, Value);
				}
			}
		}
	}
	if (MinY > MaxY)
	{
		MinY = 0.0f;
		MaxY = 1.0f;
	}
	if (MaxY - MinY < 1.0e-3f)
	{
		MinY -= 0.5f;
		MaxY += 0.5f;
	}
	const float PadX = (MaxX - MinX) * 0.05f;
	const float PadY = (MaxY - MinY) * 0.1f;
	ViewMinX = MinX - PadX;
	ViewMaxX = MaxX + PadX;
	ViewMinY = MinY - PadY;
	ViewMaxY = MaxY + PadY;
}

void FParticleCurveEditor::DeleteSelectedKeys()
{
	// 같은 트랙의 키는 큰 번호부터 지워야 앞 번호가 밀리지 않는다. 채널이 달라도 키는 하나다
	std::vector<FKeyRef> Keys = Selection;
	std::sort(Keys.begin(), Keys.end(), [](const FKeyRef& A, const FKeyRef& B)
	{
		return A.Entry != B.Entry ? A.Entry < B.Entry : (A.Track != B.Track ? A.Track < B.Track : A.Key > B.Key);
	});
	Keys.erase(std::unique(Keys.begin(), Keys.end(), [](const FKeyRef& A, const FKeyRef& B)
	{
		return A.Entry == B.Entry && A.Track == B.Track && A.Key == B.Key;
	}), Keys.end());
	for (const FKeyRef& Ref : Keys)
	{
		FEntry& Entry = Entries[Ref.Entry];
		ICurveEdTrack& Track = *Entry.Tracks[Ref.Track];
		Track.DeleteKey(Ref.Key);
		Track.AutoSetTangents();
		NotifyChanged(Entry);
	}
	Selection.clear();
}

void FParticleCurveEditor::SetSelectedInterpMode(uint8 Mode)
{
	for (const FKeyRef& Ref : Selection)
	{
		FEntry& Entry = Entries[Ref.Entry];
		ICurveEdTrack& Track = *Entry.Tracks[Ref.Track];
		Track.SetKeyInterpMode(Ref.Key, Mode);
		Track.AutoSetTangents();
		NotifyChanged(Entry);
	}
}

void FParticleCurveEditor::FlattenSelectedTangents()
{
	// UE Flatten: 기울기를 0으로 두고 직접 지정(User)으로 바꾼다
	for (const FKeyRef& Ref : Selection)
	{
		FEntry& Entry = Entries[Ref.Entry];
		ICurveEdTrack& Track = *Entry.Tracks[Ref.Track];
		Track.SetKeyInterpMode(Ref.Key, static_cast<uint8>(EInterpCurveMode::CurveUser));
		Track.SetTangent(Ref.Channel, Ref.Key, 0.0f);
		NotifyChanged(Entry);
	}
}

void FParticleCurveEditor::DrawGraph()
{
	if (bFitRequested)
	{
		FitView();
		bFitRequested = false;
	}

	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Size(std::max(ImGui::GetContentRegionAvail().x, 50.0f), std::max(ImGui::GetContentRegionAvail().y, 50.0f));
	ImGui::InvisibleButton("##CurveGraph", Size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	const bool bHovered = ImGui::IsItemHovered();
	const bool bFocused = ImGui::IsItemFocused() || ImGui::IsWindowFocused();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y), true);
	DrawList->AddRectFilled(Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y), IM_COL32(28, 28, 32, 255));

	// 커브 공간 <-> 화면
	const float ScaleX = Size.x / std::max(ViewMaxX - ViewMinX, 1.0e-6f);
	const float ScaleY = Size.y / std::max(ViewMaxY - ViewMinY, 1.0e-6f);
	const auto ToScreen = [&](float X, float Y) { return ImVec2(Origin.x + (X - ViewMinX) * ScaleX, Origin.y + Size.y - (Y - ViewMinY) * ScaleY); };
	const auto ToCurveX = [&](float ScreenX) { return ViewMinX + (ScreenX - Origin.x) / ScaleX; };
	const auto ToCurveY = [&](float ScreenY) { return ViewMinY + (Origin.y + Size.y - ScreenY) / ScaleY; };

	// 격자와 눈금 숫자. 0 축은 진하게
	const float StepX = GetGridStep(ViewMaxX - ViewMinX, Size.x, EditorUIScale::Px(60.0f));
	const float StepY = GetGridStep(ViewMaxY - ViewMinY, Size.y, EditorUIScale::Px(40.0f));
	const ImU32 GridColor = IM_COL32(60, 60, 68, 255);
	const ImU32 AxisColor = IM_COL32(110, 110, 120, 255);
	const ImU32 LabelColor = IM_COL32(150, 150, 160, 255);
	for (float X = std::ceil(ViewMinX / StepX) * StepX; X <= ViewMaxX; X += StepX)
	{
		const float ScreenX = ToScreen(X, 0.0f).x;
		DrawList->AddLine(ImVec2(ScreenX, Origin.y), ImVec2(ScreenX, Origin.y + Size.y), std::abs(X) < StepX * 0.01f ? AxisColor : GridColor);
		DrawList->AddText(ImVec2(ScreenX + 2.0f, Origin.y + Size.y - ImGui::GetTextLineHeight()), LabelColor, std::format("{:.3g}", X).c_str());
	}
	for (float Y = std::ceil(ViewMinY / StepY) * StepY; Y <= ViewMaxY; Y += StepY)
	{
		const float ScreenY = ToScreen(0.0f, Y).y;
		DrawList->AddLine(ImVec2(Origin.x, ScreenY), ImVec2(Origin.x + Size.x, ScreenY), std::abs(Y) < StepY * 0.01f ? AxisColor : GridColor);
		DrawList->AddText(ImVec2(Origin.x + 2.0f, ScreenY - ImGui::GetTextLineHeight()), LabelColor, std::format("{:.3g}", Y).c_str());
	}

	const ImVec2 Mouse = ImGui::GetMousePos();
	const float KeyRadius = EditorUIScale::Px(5.0f);
	const float TangentLength = EditorUIScale::Px(45.0f);

	// 기울기 손잡이의 화면 방향. 기울기는 "입력 1당 값 변화"라 화면 배율을 곱한다
	const auto GetTangentOffset = [&](float Tangent)
	{
		const float DX = ScaleX;
		const float DY = -Tangent * ScaleY;
		const float Length = std::sqrt(DX * DX + DY * DY);
		return ImVec2(DX / Length * TangentLength, DY / Length * TangentLength);
	};

	// 커브 선, 키, 선택 키의 기울기 손잡이를 그리면서 마우스 아래 키와 손잡이를 찾는다
	FKeyRef HoveredKey;
	FKeyRef HoveredTangent;
	bool bHoveredArrive = false;
	FKeyRef HoveredCurve; // Ctrl+클릭으로 키를 넣을 커브 (Key는 쓰지 않는다)
	for (int32 EntryIndex = 0; EntryIndex < static_cast<int32>(Entries.size()); ++EntryIndex)
	{
		FEntry& Entry = Entries[EntryIndex];
		for (int32 TrackIndex = 0; TrackIndex < static_cast<int32>(Entry.Tracks.size()); ++TrackIndex)
		{
			ICurveEdTrack& Track = *Entry.Tracks[TrackIndex];
			for (int32 Channel = 0; Channel < Track.GetNumChannels(); ++Channel)
			{
				if (!IsChannelVisible(Entry, TrackIndex, Channel))
				{
					continue;
				}
				const ImU32 Color = GetChannelColor(Entry, TrackIndex, Channel);

				// 키가 없으면 0 직선 (Eval 기본값)
				std::vector<ImVec2> Points;
				const float Step = std::max(EditorUIScale::Px(2.0f), 1.0f);
				for (float ScreenX = Origin.x; ScreenX <= Origin.x + Size.x + Step; ScreenX += Step)
				{
					const float X = ToCurveX(ScreenX);
					Points.push_back(ToScreen(X, Track.Eval(Channel, X)));
				}
				DrawList->AddPolyline(Points.data(), static_cast<int>(Points.size()), Color, ImDrawFlags_None, EditorUIScale::Px(1.5f));

				if (bHovered && std::abs(ToScreen(0.0f, Track.Eval(Channel, ToCurveX(Mouse.x))).y - Mouse.y) < KeyRadius + 2.0f)
				{
					HoveredCurve = { EntryIndex, TrackIndex, Channel, -1 };
				}

				for (int32 Key = 0; Key < Track.GetNumKeys(); ++Key)
				{
					const FKeyRef Ref{ EntryIndex, TrackIndex, Channel, Key };
					const ImVec2 KeyPos = ToScreen(Track.GetKeyIn(Key), Track.GetKeyOut(Channel, Key));
					const bool bSelected = IsSelected(Ref);

					// 곡선 보간 키를 골랐으면 기울기 손잡이 두 개 (UE와 같이 앞, 뒤)
					const EInterpCurveMode Mode = static_cast<EInterpCurveMode>(Track.GetKeyInterpMode(Key));
					if (bSelected && (Mode == EInterpCurveMode::CurveAuto || Mode == EInterpCurveMode::CurveUser))
					{
						const ImVec2 Offset = GetTangentOffset(Track.GetTangent(Channel, Key));
						const ImVec2 Arrive(KeyPos.x - Offset.x, KeyPos.y - Offset.y);
						const ImVec2 Leave(KeyPos.x + Offset.x, KeyPos.y + Offset.y);
						DrawList->AddLine(Arrive, Leave, IM_COL32(200, 200, 200, 200));
						DrawList->AddCircleFilled(Arrive, KeyRadius * 0.7f, IM_COL32(220, 220, 220, 255));
						DrawList->AddCircleFilled(Leave, KeyRadius * 0.7f, IM_COL32(220, 220, 220, 255));
						const auto Near = [&](const ImVec2& P) { return std::abs(P.x - Mouse.x) <= KeyRadius && std::abs(P.y - Mouse.y) <= KeyRadius; };
						if (bHovered && Near(Arrive))
						{
							HoveredTangent = Ref;
							bHoveredArrive = true;
						}
						else if (bHovered && Near(Leave))
						{
							HoveredTangent = Ref;
							bHoveredArrive = false;
						}
					}

					const ImVec2 Min(KeyPos.x - KeyRadius, KeyPos.y - KeyRadius);
					const ImVec2 Max(KeyPos.x + KeyRadius, KeyPos.y + KeyRadius);
					DrawList->AddRectFilled(Min, Max, bSelected ? IM_COL32(255, 255, 255, 255) : Color);
					DrawList->AddRect(Min, Max, IM_COL32(0, 0, 0, 255));
					if (bHovered && Mouse.x >= Min.x && Mouse.x <= Max.x && Mouse.y >= Min.y && Mouse.y <= Max.y)
					{
						HoveredKey = Ref;
					}
				}
			}
		}
	}

	// ---- 입력 ----
	const ImGuiIO& IO = ImGui::GetIO();
	if (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		if (HoveredTangent.Entry >= 0)
		{
			DragMode = EDragMode::Tangent;
			DragTangentKey = HoveredTangent;
			bDragArriveTangent = bHoveredArrive;
			bDragChanged = false;
		}
		else
		{
			FKeyRef Pressed = HoveredKey;
			// UE: Ctrl+클릭으로 커브 위에 키를 넣는다
			if (Pressed.Entry < 0 && IO.KeyCtrl && HoveredCurve.Entry >= 0)
			{
				FEntry& Entry = Entries[HoveredCurve.Entry];
				ICurveEdTrack& Track = *Entry.Tracks[HoveredCurve.Track];
				const int32 NewKey = Track.CreateNewKey(ToCurveX(Mouse.x));
				if (NewKey >= 0)
				{
					Track.AutoSetTangents();
					NotifyChanged(Entry);
					Pressed = { HoveredCurve.Entry, HoveredCurve.Track, HoveredCurve.Channel, NewKey };
					Selection.clear();
				}
			}

			if (Pressed.Entry >= 0)
			{
				// Ctrl은 선택에 더하고 빼기. 아니면 이 키만 (이미 고른 키를 누르면 여러 개를 함께 끈다)
				if (IO.KeyCtrl && HoveredKey.Entry >= 0)
				{
					const auto Found = std::find(Selection.begin(), Selection.end(), Pressed);
					if (Found != Selection.end())
					{
						Selection.erase(Found);
					}
					else
					{
						Selection.push_back(Pressed);
					}
				}
				else if (!IsSelected(Pressed))
				{
					Selection = { Pressed };
				}
				DragMode = EDragMode::Keys;
				bDragChanged = false;
				DragStartX = ToCurveX(Mouse.x);
				DragStartY = ToCurveY(Mouse.y);
				DragStartIn.clear();
				DragStartOut.clear();
				for (const FKeyRef& Ref : Selection)
				{
					const ICurveEdTrack& Track = *Entries[Ref.Entry].Tracks[Ref.Track];
					DragStartIn.push_back(Track.GetKeyIn(Ref.Key));
					DragStartOut.push_back(Track.GetKeyOut(Ref.Channel, Ref.Key));
				}
			}
			else
			{
				Selection.clear();
			}
		}
	}
	if (bHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
	{
		DragMode = EDragMode::Pan;
	}

	if (DragMode == EDragMode::Keys && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float DeltaX = ToCurveX(Mouse.x) - DragStartX;
		const float DeltaY = ToCurveY(Mouse.y) - DragStartY;
		if (DeltaX != 0.0f || DeltaY != 0.0f)
		{
			for (size_t Index = 0; Index < Selection.size(); ++Index)
			{
				const FKeyRef& Ref = Selection[Index];
				FEntry& Entry = Entries[Ref.Entry];
				ICurveEdTrack& Track = *Entry.Tracks[Ref.Track];
				Track.SetKeyIn(Ref.Key, DragStartIn[Index] + DeltaX);
				Track.SetKeyOut(Ref.Channel, Ref.Key, DragStartOut[Index] + DeltaY);
				Track.AutoSetTangents();
				Entry.Distribution->MarkDirty();
			}
			bDragChanged = true;
		}
	}
	else if (DragMode == EDragMode::Tangent && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		FEntry& Entry = Entries[DragTangentKey.Entry];
		ICurveEdTrack& Track = *Entry.Tracks[DragTangentKey.Track];
		const ImVec2 KeyPos = ToScreen(Track.GetKeyIn(DragTangentKey.Key), Track.GetKeyOut(DragTangentKey.Channel, DragTangentKey.Key));
		float RelX = Mouse.x - KeyPos.x;
		float RelY = Mouse.y - KeyPos.y;
		if (bDragArriveTangent)
		{
			RelX = -RelX;
			RelY = -RelY;
		}
		// 손잡이가 키를 넘어가면 기울기가 무한대가 되므로 막는다
		RelX = std::max(RelX, 1.0f);
		const float Tangent = -(RelY / ScaleY) / (RelX / ScaleX);
		// UE: 손잡이를 끌면 Auto 키도 직접 지정(User)으로 바뀐다
		Track.SetKeyInterpMode(DragTangentKey.Key, static_cast<uint8>(EInterpCurveMode::CurveUser));
		Track.SetTangent(DragTangentKey.Channel, DragTangentKey.Key, Tangent);
		Entry.Distribution->MarkDirty();
		bDragChanged = true;
	}
	else if (DragMode == EDragMode::Pan && (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)))
	{
		const float MoveX = IO.MouseDelta.x / ScaleX;
		const float MoveY = IO.MouseDelta.y / ScaleY;
		ViewMinX -= MoveX;
		ViewMaxX -= MoveX;
		ViewMinY += MoveY;
		ViewMaxY += MoveY;
	}
	else if (DragMode != EDragMode::None)
	{
		// 끌기를 끝낸 순간에 한 번만 알린다 (저장 표시)
		if (bDragChanged && OnChanged)
		{
			OnChanged();
		}
		DragMode = EDragMode::None;
		bDragChanged = false;
	}

	// 휠 확대 / 축소. 마우스 위치를 중심으로. Ctrl은 가로만, Shift는 세로만
	if (bHovered && IO.MouseWheel != 0.0f)
	{
		const float Factor = std::pow(0.85f, IO.MouseWheel);
		const float CenterX = ToCurveX(Mouse.x);
		const float CenterY = ToCurveY(Mouse.y);
		if (!IO.KeyShift)
		{
			ViewMinX = CenterX + (ViewMinX - CenterX) * Factor;
			ViewMaxX = CenterX + (ViewMaxX - CenterX) * Factor;
		}
		if (!IO.KeyCtrl)
		{
			ViewMinY = CenterY + (ViewMinY - CenterY) * Factor;
			ViewMaxY = CenterY + (ViewMaxY - CenterY) * Factor;
		}
	}

	// 키: Delete = 선택 키 삭제, F = 맞춤
	if (bFocused && !IO.WantTextInput && DragMode == EDragMode::None)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !Selection.empty())
		{
			DeleteSelectedKeys();
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_F, false))
		{
			FitView();
		}
	}

	if (bHovered && HoveredKey.Entry >= 0 && DragMode == EDragMode::None)
	{
		const ICurveEdTrack& Track = *Entries[HoveredKey.Entry].Tracks[HoveredKey.Track];
		ImGui::SetTooltip("%s\nTime %.3f  Value %.3f  (%s)", Entries[HoveredKey.Entry].Name.c_str(), Track.GetKeyIn(HoveredKey.Key),
			Track.GetKeyOut(HoveredKey.Channel, HoveredKey.Key), GetInterpModeName(Track.GetKeyInterpMode(HoveredKey.Key)));
	}
	DrawList->PopClipRect();
}
