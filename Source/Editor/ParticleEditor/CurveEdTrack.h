#pragma once

#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include "Math/InterpCurve.h"

#include <algorithm>
#include <cfloat>
#include <memory>
#include <vector>

// 커브 하나(FInterpCurve<T>)를 값 종류와 상관없이 채널 단위로 읽고 쓰는 창구 (UE FCurveEdInterface).
// float는 채널 1개, FVector2(Uniform 최소/최대)는 2개, FVector는 3개.
// Curve Editor 그래프와 Details 키 표가 같이 쓴다
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

	// 이웃 키를 넘지 않게 제한한다 (순서 유지)
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

namespace CurveEd
{
	// 값 종류별 채널 읽기 / 쓰기
	inline float GetChannel(const float& Value, int32) { return Value; }
	inline float GetChannel(const FVector2& Value, int32 Channel) { return Channel == 0 ? Value.X : Value.Y; }
	inline float GetChannel(const FVector& Value, int32 Channel) { return Channel == 0 ? Value.X : (Channel == 1 ? Value.Y : Value.Z); }
	inline void SetChannel(float& Value, int32, float NewValue) { Value = NewValue; }
	inline void SetChannel(FVector2& Value, int32 Channel, float NewValue) { (Channel == 0 ? Value.X : Value.Y) = NewValue; }
	inline void SetChannel(FVector& Value, int32 Channel, float NewValue) { (Channel == 0 ? Value.X : (Channel == 1 ? Value.Y : Value.Z)) = NewValue; }
	template<typename T> constexpr int32 GetNumChannelsOf() { return 1; }
	template<> constexpr int32 GetNumChannelsOf<FVector2>() { return 2; }
	template<> constexpr int32 GetNumChannelsOf<FVector>() { return 3; }

	// 키 사이 최소 간격. 키 시간이 같아지면 구간 길이가 0이 된다
	constexpr float MinKeyGap = 1.0e-4f;

	// 보간 방식 이름. EInterpCurveMode 순서와 같다
	inline const char* GetInterpModeName(uint8 Mode)
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

	template<typename T>
	class TCurveEdTrack : public ICurveEdTrack
	{
	public:
		explicit TCurveEdTrack(FInterpCurve<T>& InCurve) : Curve(InCurve) {}

		int32 GetNumKeys() const override { return Curve.Points.Num(); }
		int32 GetNumChannels() const override { return GetNumChannelsOf<T>(); }
		float GetKeyIn(int32 KeyIndex) const override { return Curve.Points[KeyIndex].InVal; }
		float GetKeyOut(int32 Channel, int32 KeyIndex) const override { return GetChannel(Curve.Points[KeyIndex].OutVal, Channel); }
		float Eval(int32 Channel, float InVal) const override { return GetChannel(Curve.Eval(InVal), Channel); }
		uint8 GetKeyInterpMode(int32 KeyIndex) const override { return static_cast<uint8>(Curve.Points[KeyIndex].InterpMode); }
		float GetTangent(int32 Channel, int32 KeyIndex) const override { return GetChannel(Curve.Points[KeyIndex].LeaveTangent, Channel); }

		void SetKeyIn(int32 KeyIndex, float InVal) override
		{
			const float Low = KeyIndex > 0 ? Curve.Points[KeyIndex - 1].InVal + MinKeyGap : -FLT_MAX;
			const float High = KeyIndex + 1 < Curve.Points.Num() ? Curve.Points[KeyIndex + 1].InVal - MinKeyGap : FLT_MAX;
			Curve.Points[KeyIndex].InVal = std::clamp(InVal, Low, std::max(Low, High));
		}
		void SetKeyOut(int32 Channel, int32 KeyIndex, float Value) override { SetChannel(Curve.Points[KeyIndex].OutVal, Channel, Value); }
		void SetKeyInterpMode(int32 KeyIndex, uint8 Mode) override { Curve.Points[KeyIndex].InterpMode = static_cast<EInterpCurveMode>(Mode); }
		void SetTangent(int32 Channel, int32 KeyIndex, float Tangent) override
		{
			SetChannel(Curve.Points[KeyIndex].ArriveTangent, Channel, Tangent);
			SetChannel(Curve.Points[KeyIndex].LeaveTangent, Channel, Tangent);
		}
		int32 CreateNewKey(float InVal) override
		{
			const T Value = Curve.Eval(InVal, T());
			return Curve.AddPoint(InVal, Value, EInterpCurveMode::CurveAuto);
		}
		void DeleteKey(int32 KeyIndex) override { Curve.RemovePoint(KeyIndex); }
		void AutoSetTangents() override { Curve.AutoSetTangents(); }

	private:
		FInterpCurve<T>& Curve;
	};

	// 커브 분포면 트랙을 만든다 (Uniform Vector는 Min, Max 두 개). 아니면 비어 있다
	inline void MakeTracks(UDistribution* Distribution, std::vector<std::unique_ptr<ICurveEdTrack>>& OutTracks, std::vector<FString>& OutNames)
	{
		if (auto* FloatCurve = Cast<UDistributionFloatConstantCurve>(Distribution))
		{
			OutTracks.push_back(std::make_unique<TCurveEdTrack<float>>(FloatCurve->ConstantCurve));
			OutNames.push_back("");
		}
		else if (auto* FloatUniformCurve = Cast<UDistributionFloatUniformCurve>(Distribution))
		{
			OutTracks.push_back(std::make_unique<TCurveEdTrack<FVector2>>(FloatUniformCurve->ConstantCurve));
			OutNames.push_back("");
		}
		else if (auto* VectorCurve = Cast<UDistributionVectorConstantCurve>(Distribution))
		{
			OutTracks.push_back(std::make_unique<TCurveEdTrack<FVector>>(VectorCurve->ConstantCurve));
			OutNames.push_back("");
		}
		else if (auto* VectorUniformCurve = Cast<UDistributionVectorUniformCurve>(Distribution))
		{
			OutTracks.push_back(std::make_unique<TCurveEdTrack<FVector>>(VectorUniformCurve->MinCurve));
			OutNames.push_back("Min");
			OutTracks.push_back(std::make_unique<TCurveEdTrack<FVector>>(VectorUniformCurve->MaxCurve));
			OutNames.push_back("Max");
		}
	}

	inline bool IsCurveDistribution(UDistribution* Distribution)
	{
		return Cast<UDistributionFloatConstantCurve>(Distribution) || Cast<UDistributionFloatUniformCurve>(Distribution)
			|| Cast<UDistributionVectorConstantCurve>(Distribution) || Cast<UDistributionVectorUniformCurve>(Distribution);
	}
}
