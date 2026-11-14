#pragma once

#include "Particles/ParticleModule.h"
#include "Render/Material.h"

// 반투명 파티클을 그리기 전에 정렬하는 기준
enum EParticleSortMode : int32
{
	PSORTMODE_None,
	PSORTMODE_ViewProjDepth,
	PSORTMODE_DistanceToView,
	PSORTMODE_Age_OldestFirst,
	PSORTMODE_Age_NewestFirst,
};

// 스프라이트가 카메라를 어떻게 향할지
enum EParticleScreenAlignment : int32
{
	PSA_FacingCameraPosition,
	PSA_Square,
	PSA_Rectangle,
	PSA_Velocity,
	PSA_AwayFromCenter,
	PSA_TypeSpecific,
	PSA_FacingCameraDistanceBlend,
};

// 모든 이미터에 반드시 하나 있는 모듈
// 이미터 전체에 적용되는 설정(머티리얼, 정렬, 공간, 이미터 수명 등)을 담는다.
// LOD 레벨의 Modules 목록에 들어가지 않고 RequiredModule 포인터로 따로 보관한다.
class UParticleModuleRequired : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleRequired, UParticleModule)

	REFLECT_START(ClassName)
		PROPERTY(Material)
		PROPERTY(bUseLocalSpace)
		PROPERTY(bKillOnDeactivate)
		PROPERTY(bKillOnCompleted)
		PROPERTY(EmitterDuration)
		PROPERTY(EmitterDurationLow)
		PROPERTY(bEmitterDurationUseRange)
		PROPERTY(bDurationRecalcEachLoop)
		PROPERTY(EmitterLoops)
		PROPERTY(EmitterDelay)
		PROPERTY(EmitterDelayLow)
		PROPERTY(bEmitterDelayUseRange)
		PROPERTY(bDelayFirstLoopOnly)
		PROPERTY_ENUM(ScreenAlignment, "Facing Camera Position", "Square", "Rectangle", "Velocity", "Away From Center", "Type Specific", "Facing Camera Distance Blend")
		PROPERTY_ENUM(SortMode, "None", "View Proj Depth", "Distance To View", "Age Oldest First", "Age Newest First")
		PROPERTY(SubImages_Horizontal)
		PROPERTY(SubImages_Vertical)
		PROPERTY(bUseMaxDrawCount)
		PROPERTY(MaxDrawCount)
	REFLECT_END()

public:
	UParticleModuleRequired() = default;
	virtual ~UParticleModuleRequired() = default;

public:
	UMaterial* Material = nullptr;

	// true면 파티클이 컴포넌트를 따라 움직인다 (컴포넌트 공간에서 시뮬레이션).
	bool bUseLocalSpace = false;
	// Deactivate 때 살아 있는 파티클을 바로 지울지
	bool bKillOnDeactivate = false;
	// 모든 루프가 끝나면 살아 있는 파티클을 바로 지울지
	bool bKillOnCompleted = false;

	// 한 루프의 길이 (초)
	float EmitterDuration = 1.0f;
	float EmitterDurationLow = 0.0f;
	bool bEmitterDurationUseRange = false;
	bool bDurationRecalcEachLoop = false;
	// 반복 횟수. 0이면 무한
	int32 EmitterLoops = 0;

	// 루프 시작 전 대기 시간 (초)
	float EmitterDelay = 0.0f;
	float EmitterDelayLow = 0.0f;
	bool bEmitterDelayUseRange = false;
	bool bDelayFirstLoopOnly = false;

	// 렌더 설정 (6, 7단계에서 사용)
	EParticleScreenAlignment ScreenAlignment = PSA_Square;
	EParticleSortMode SortMode = PSORTMODE_None;
	// SubUV 텍스처의 가로·세로 칸 수
	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;
	// 한 번에 그릴 최대 파티클 수
	bool bUseMaxDrawCount = false;
	int32 MaxDrawCount = 500;
};
