#pragma once

#include <functional>
#include <memory>
#include <vector>

class UParticleSystem;
class UParticleModule;
class UDistribution;

// 커브 하나(FInterpCurve<T>)를 값 종류와 상관없이 채널 단위로 읽고 쓰는 창구 (UE FCurveEdInterface).
// float는 채널 1개, FVector2(Uniform 최소/최대)는 2개, FVector는 3개
class ICurveEdTrack
{
public:
	virtual ~ICurveEdTrack() = default;
	virtual int32 GetNumKeys() const = 0;
	virtual int32 GetNumChannels() const = 0;
	virtual float GetKeyIn(int32 KeyIndex) const = 0;
	virtual float GetKeyOut(int32 Channel, int32 KeyIndex) const = 0;
	virtual float Eval(int32 Channel, float InVal) const = 0;
	virtual uint8 GetKeyInterpMode(int32 KeyIndex) const = 0;
	virtual float GetTangent(int32 Channel, int32 KeyIndex) const = 0;

	virtual void SetKeyIn(int32 KeyIndex, float InVal) = 0;
	virtual void SetKeyOut(int32 Channel, int32 KeyIndex, float Value) = 0;
	virtual void SetKeyInterpMode(int32 KeyIndex, uint8 Mode) = 0;
	// 들어오는 기울기와 나가는 기울기를 같게 둔다 (UE CurveUser)
	virtual void SetTangent(int32 Channel, int32 KeyIndex, float Tangent) = 0;
	// 그 위치의 지금 값으로 키를 넣는다. 커브 모양은 바뀌지 않는다. 들어간 자리를 돌려준다
	virtual int32 CreateNewKey(float InVal) = 0;
	virtual void DeleteKey(int32 KeyIndex) = 0;
	// CurveAuto 키의 기울기를 이웃 키로 다시 계산한다
	virtual void AutoSetTangents() = 0;
};

// Cascade 아래쪽의 Curve Editor (UE FCascade::CurveEditor).
// 모듈의 그래프 버튼으로 그 모듈의 커브 분포를 올리고, 키를 보고 옮기고 추가하고 지운다.
// 커브는 매 프레임 그대로 읽히므로 인스턴스를 다시 만들 필요가 없다. 바뀌면 OnChanged로 알린다 (저장 표시)
class FParticleCurveEditor
{
public:
	void SetSystem(UParticleSystem* InSystem) { System = InSystem; }
	void SetOnChanged(std::function<void()> InOnChanged) { OnChanged = std::move(InOnChanged); }

	// 모듈에 커브 분포가 하나라도 있으면 true (그래프 버튼을 보일지)
	static bool HasCurves(UParticleModule* Module);
	bool HasModule(const UParticleModule* Module) const;
	// 모듈의 커브 분포를 모두 올리거나, 이미 있으면 내린다 (UE 모듈의 그래프 버튼)
	void ToggleModule(UParticleModule* Module, const FString& ModuleName);
	// 에셋을 다시 읽는 등 모듈이 모두 바뀔 때
	void Clear();

	// 패널 내용: 툴바 + 왼쪽 커브 목록 + 오른쪽 그래프
	void Draw();

private:
	// 그래프에 올라간 분포 하나. Uniform Vector는 최소/최대 커브 두 개(Track)를 갖는다
	struct FEntry
	{
		UParticleModule* Module = nullptr;
		UDistribution* Distribution = nullptr;
		FString Name;
		uint32 Color = 0;
		bool bVisible = true;
		bool bChannelVisible[3] = { true, true, true };
		std::vector<std::unique_ptr<ICurveEdTrack>> Tracks;
		std::vector<FString> TrackNames;
	};

	// 선택한 키. 키의 시간(InVal)은 모든 채널이 같이 쓰고, 값은 채널마다 따로다
	struct FKeyRef
	{
		int32 Entry = -1;
		int32 Track = -1;
		int32 Channel = -1;
		int32 Key = -1;
		bool operator==(const FKeyRef& Other) const
		{
			return Entry == Other.Entry && Track == Other.Track && Channel == Other.Channel && Key == Other.Key;
		}
	};

	enum class EDragMode
	{
		None,
		Keys,
		Tangent,
		Pan,
	};

	// 모듈이 지워졌거나 Details에서 분포 종류를 바꿔 옛 분포가 지워졌으면 목록에서 뺀다. 포인터를 읽기 전에 시스템에서 찾는다
	void RemoveStaleEntries();
	void DrawToolbar();
	void DrawCurveList(float Width);
	void DrawGraph();

	void FitView();
	void NotifyChanged(FEntry& Entry);
	void DeleteSelectedKeys();
	void SetSelectedInterpMode(uint8 Mode);
	void FlattenSelectedTangents();

	bool IsChannelVisible(const FEntry& Entry, int32 Track, int32 Channel) const;
	uint32 GetChannelColor(const FEntry& Entry, int32 Track, int32 Channel) const;
	bool IsSelected(const FKeyRef& Ref) const;

	UParticleSystem* System = nullptr;
	std::function<void()> OnChanged;
	std::vector<FEntry> Entries;
	std::vector<FKeyRef> Selection;

	// 보이는 범위. X = 커브 입력(보통 파티클 RelativeTime 0~1), Y = 값
	float ViewMinX = 0.0f;
	float ViewMaxX = 1.0f;
	float ViewMinY = 0.0f;
	float ViewMaxY = 1.0f;
	bool bFitRequested = false;

	EDragMode DragMode = EDragMode::None;
	bool bDragChanged = false;
	float DragStartX = 0.0f;
	float DragStartY = 0.0f;
	// 키 드래그를 시작할 때의 선택 키 위치 (Selection과 같은 순서)
	std::vector<float> DragStartIn;
	std::vector<float> DragStartOut;
	FKeyRef DragTangentKey;
	bool bDragArriveTangent = false;
};
