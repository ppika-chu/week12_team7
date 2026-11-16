#pragma once

#include "Core/Types.h"
#include "Math/Color.h"
#include <cstddef>
#include <type_traits>

// 스프라이트 한 개의 GPU 입력 데이터
struct FParticleSpriteInstanceData
{
	FVector Center = FVector::ZeroVector;
	float Rotation = 0.0f;	// 빌보드 스프라이트 회전 각도
	FVector2 Size = FVector2(1.0f, 1.0f);
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};

static_assert(std::is_standard_layout_v<FParticleSpriteInstanceData>);
static_assert(std::is_trivially_copyable_v<FParticleSpriteInstanceData>);
static_assert(sizeof(FParticleSpriteInstanceData) == 40);
static_assert(offsetof(FParticleSpriteInstanceData, Center) == 0);
static_assert(offsetof(FParticleSpriteInstanceData, Rotation) == 12);
static_assert(offsetof(FParticleSpriteInstanceData, Size) == 16);
static_assert(offsetof(FParticleSpriteInstanceData, Color) == 24);