#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"
#include "Distributions/DistributionVector.h"
#include <type_traits>

class UParticleLODLevel;

enum class EParticleCollisionComplete : int32
{
	Kill,				// 제거
	FreezeTranslation,	// 이동만 정지
	FreezeMovement,		// 이동,회전 정지
	HaltCollisions		// 충돌 검사 종료
};

struct FParticleCollisionPayload
{
	FVector DampingFactor;		// 속도에 곱할 값
	int32 RemainingCollisions;	// 남은 충돌 횟수
	uint32 bInitialized;		// 초기화 여부
};

static_assert(sizeof(FParticleCollisionPayload) == 20);
static_assert(alignof(FParticleCollisionPayload) == alignof(float));
static_assert(std::is_trivially_copyable_v<FParticleCollisionPayload>);
static_assert(std::is_trivially_destructible_v<FParticleCollisionPayload>);

class UParticleModuleCollision : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleCollision, UParticleModule)

	REFLECT_START(UParticleModuleCollision)
		CLASS_TOOLTIP("Bounce or stop on hits and send collision events.")
		PROPERTY_DISTRIBUTION(DampingFactor) PROPERTY_TOOLTIP("Velocity multiplier on hit, per axis. 1 = full bounce, 0 = stop.")
		PROPERTY_DISTRIBUTION(MaxCollisions) PROPERTY_TOOLTIP("Hits before Completion Option runs.")
		PROPERTY_ENUM(CompletionOption, "Kill", "Stop Translation", "Stop Movement", "End Collision Check") PROPERTY_TOOLTIP("After the last hit.\nKill: remove\nStop Translation: stop moving\nStop Movement: freeze\nEnd Collision Check: stop checking hits")
		PROPERTY(CollisionOffset) PROPERTY_TOOLTIP("Gap kept from the hit surface (m). Prevents jitter.")
	REFLECT_END()

public:
	UParticleModuleCollision();

	void Spawn(const FSpawnContext& Context) override;
	void FinalUpdate(const FUpdateContext& Context) override;
	uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) override;

	static bool ValidateLOD(const UParticleLODLevel& LODLevel, FString& Error);

	FRawDistributionVector DampingFactor;	// payload에 전달할 DampingFactor를 뽑는 Distribution
	FRawDistributionFloat MaxCollisions;
	EParticleCollisionComplete CompletionOption = EParticleCollisionComplete::Kill;
	float CollisionOffset = 0.01f; // 충돌 후 이격 거리
	float DirScalar = 3.5f;        // 검사 끝점 보정

private:
	bool CanRun(const FParticleEmitterInstance& Owner, int32 Offset) const;
	void InitializePayload(FParticleEmitterInstance& Owner, FParticleCollisionPayload& Payload) const;
	FVector ComputeBounceVelocity(const FVector& Velocity, const FVector& Normal, const FVector& Damping) const;
	bool ApplyCompletion(FBaseParticle& Particle) const;
};
