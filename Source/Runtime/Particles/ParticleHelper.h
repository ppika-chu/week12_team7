#pragma once

#include "Core/Types.h"
#include "Math/Color.h"
#include <cstddef>
#include <type_traits>

// 파티클 한 개의 시뮬레이션 상태
struct FBaseParticle
{
	FVector Location = FVector(0.0f);
	FVector OldLocation = FVector(0.0f);
	FVector Velocity = FVector(0.0f);
	FVector BaseVelocity = FVector(0.0f);
	FVector Size = FVector(1.0f);
	FVector BaseSize = FVector(1.0f);
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	FLinearColor BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	float RelativeTime = 0.0f;
	float OneOverMaxLifetime = 0.0f;	// 전체 수명의 역수
	float Rotation = 0.0f;
	float RotationRate = 0.0f;
	uint32 ParticleId = 0;
};

static_assert(std::is_trivially_copyable_v<FBaseParticle>);
static_assert(std::is_trivially_destructible_v<FBaseParticle>);
static_assert(alignof(FBaseParticle) <= alignof(std::max_align_t));

// 파티클 데이터 블록의 소유와 관리
struct FParticleDataContainer
{
	static constexpr uint32 Alignment = alignof(std::max_align_t);
	static constexpr int32 MaxCapacity = 1 << 16;

	FParticleDataContainer() = default;
	~FParticleDataContainer();
	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

	// 메모리 배치 검증과 새 블록 할당
	bool Allocate(int32 Capacity, int32 Stride);
	// 소유 블록 해제와 내부 포인터 초기화
	void Reset();

	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr;
	uint16* ParticleIndices = nullptr;
};

// 활성 배열 위치에 대응하는 파티클 참조
#define DECLARE_PARTICLE_PTR(Owner, ActiveIndex) \
	FBaseParticle& Particle = (Owner)->GetParticle(ActiveIndex)

// 현재 파티클의 삭제를 허용하는 역순 순회
#define BEGIN_UPDATE_LOOP(Owner)                                                                  \
	for (int32 ParticleIndex = (Owner)->ActiveParticles - 1; ParticleIndex >= 0; --ParticleIndex) \
	{                                                                                             \
		DECLARE_PARTICLE_PTR(Owner, ParticleIndex);

// 역순 순회 본문의 종료
#define END_UPDATE_LOOP \
	}