#pragma once

#include "Math/Vector.h"
#include "Math/Color.h"

// 파티클 한 개의 공통 데이터
// 모듈별 추가 데이터(payload)는 이 구조체 바로 뒤에 붙는다.

// 16바이트 단위로 묶어 전체 크기를 16의 배수로 유지한다.
struct FBaseParticle
{
	// 16 bytes
	FVector OldLocation;      // 지난 프레임 위치. 속도 방향 정렬과 충돌에 쓴다.
	int32 Flags;              // 상태 플래그와 생성 순번

	// 16 bytes
	FVector Location;         // 현재 위치
	float Rotation;           // 회전 (라디안)

	// 16 bytes
	FVector BaseVelocity;     // 매 프레임 시작 시 Velocity = BaseVelocity
	float BaseRotationRate;   // 초기 회전 속도 (라디안/초)

	// 16 bytes
	FVector Velocity;         // 현재 속도
	float RotationRate;       // 매 프레임 시작 시 RotationRate = BaseRotationRate

	// 16 bytes
	FVector BaseSize;         // 매 프레임 시작 시 Size = BaseSize
	float RelativeTime;       // 0 = 생성, 1 = 소멸

	// 16 bytes
	FVector Size;             // 현재 크기
	float OneOverMaxLifetime; // 1 / 수명. RelativeTime 증가량 계산에 쓴다.

	// 16 bytes
	FLinearColor Color;       // 현재 색

	// 16 bytes
	FLinearColor BaseColor;   // 매 프레임 시작 시 Color = BaseColor
};
static_assert(sizeof(FBaseParticle) % 16 == 0, "FBaseParticle의 크기는 16의 배수여야 한다.");
