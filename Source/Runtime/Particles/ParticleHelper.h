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

// FBaseParticle::Flags. 상위 7비트는 상태, 나머지는 생성 순번
enum EParticleStates : uint32
{
	STATE_Particle_JustSpawned       = 0x02000000, // 이번 프레임에 생성됨
	STATE_Particle_Freeze            = 0x04000000, // 업데이트 정지
	STATE_Particle_FreezeTranslation = 0x10000000, // 이동 정지
	STATE_Particle_FreezeRotation    = 0x20000000, // 회전 정지
	STATE_Mask                       = 0xFE000000,
	STATE_CounterMask                = ~STATE_Mask,
};

// 파티클 접근 매크로 (UE ParticleHelper.h)
#define DECLARE_PARTICLE(Name, Address) \
	FBaseParticle& Name = *((FBaseParticle*)(Address));

#define DECLARE_PARTICLE_CONST(Name, Address) \
	const FBaseParticle& Name = *((const FBaseParticle*)(Address));

#define DECLARE_PARTICLE_PTR(Name, Address) \
	FBaseParticle* Name = (FBaseParticle*)(Address);

// Update 모듈에서 살아 있는 파티클 전체를 뒤에서부터 순회한다.
#define BEGIN_UPDATE_LOOP                                                             \
	{                                                                                 \
		int32& ActiveParticles = Context.Owner.ActiveParticles;                       \
		int32 Offset = Context.Offset;                                                \
		uint32 CurrentOffset = Offset;                                                \
		float DeltaTime = Context.DeltaTime;                                          \
		const uint8* ParticleData = Context.Owner.ParticleData;                       \
		const uint32 ParticleStride = Context.Owner.ParticleStride;                   \
		uint16* ParticleIndices = Context.Owner.ParticleIndices;                      \
		for (int32 i = ActiveParticles - 1; i >= 0; i--)                              \
		{                                                                             \
			const int32 CurrentIndex = ParticleIndices[i];                            \
			const uint8* ParticleBase = ParticleData + CurrentIndex * ParticleStride; \
			FBaseParticle& Particle = *((FBaseParticle*)ParticleBase);                \
			if ((Particle.Flags & STATE_Particle_Freeze) == 0)                        \
			{

#define END_UPDATE_LOOP          \
			}                    \
			CurrentOffset = Offset; \
		}                        \
	}

#define CONTINUE_UPDATE_LOOP \
	CurrentOffset = Offset;  \
	continue;

// Spawn 모듈에서 생성 중인 파티클 하나에 접근한다.
#define SPAWN_INIT                                                 \
	const int32 ActiveParticles = Context.Owner.ActiveParticles;   \
	const uint32 ParticleStride = Context.Owner.ParticleStride;    \
	uint32 CurrentOffset = Context.Offset;                         \
	FBaseParticle* ParticleBase = Context.ParticleBase;            \
	FBaseParticle& Particle = *(ParticleBase);

// 현재 위치(CurrentOffset)의 payload를 Type으로 꺼내고 위치를 그 크기만큼 옮긴다.
#define PARTICLE_ELEMENT(Type, Name)                                 \
	Type& Name = *((Type*)((uint8*)ParticleBase + CurrentOffset));   \
	CurrentOffset += sizeof(Type);

// BEGIN_UPDATE_LOOP 안에서 현재 파티클을 지운다.
#define KILL_CURRENT_PARTICLE                                         \
	{                                                                 \
		ParticleIndices[i] = ParticleIndices[ActiveParticles - 1];    \
		ParticleIndices[ActiveParticles - 1] = CurrentIndex;          \
		ActiveParticles--;                                            \
	}
