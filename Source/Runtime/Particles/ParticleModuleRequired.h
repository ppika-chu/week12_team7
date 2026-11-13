#pragma once

#include "Particles/ParticleModule.h"
#include "Render/Material.h"

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
};
