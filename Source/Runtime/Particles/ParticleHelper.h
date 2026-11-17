#pragma once

#include "Math/Vector.h"
#include "Math/Vector2.h"
#include "Math/Color.h"
#include <cstddef>
#include <type_traits>

class UMaterial;
class UParticleModuleRequired;

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

static_assert(std::is_trivially_copyable_v<FBaseParticle>);
static_assert(std::is_trivially_destructible_v<FBaseParticle>);

// FBaseParticle::Flags. 상위 7비트는 상태, 나머지는 생성 순번
enum EParticleStates : uint32
{
	STATE_Particle_JustSpawned       = 0x02000000, // 이번 프레임에 생성됨
	STATE_Particle_Freeze            = 0x04000000, // 업데이트 정지
	STATE_Particle_FreezeTranslation = 0x10000000, // 이동 정지
	STATE_Particle_FreezeRotation    = 0x20000000, // 회전 정지
	STATE_Mask                       = 0xFE000000,
	STATE_Particle_IgnoreCollisions  = 0x08000000, // 충돌 검사 종료
	STATE_CounterMask                = ~STATE_Mask,
};

// 충돌 후 외부에 전달할 구조체
struct FParticleCollisionEvent
{
	uint32 SourceComponentUUID = 0;	// 이벤트 발생 PSC UUID
	uint32 EmitterUUID = 0;			// 에셋 이미터 UUID
	uint32 ParticleId = 0;			// 파티클 순번
	uint32 HitActorUUID = 0;		// 충돌 대상 액터 UUID
	uint32 HitComponentUUID = 0;	// 충돌 대상 컴포넌트 UUID

	FVector Location = FVector::ZeroVector;	// 월드 충돌 위치
	FVector Normal = FVector::ZeroVector;	// 월드 충돌 표면 법선
	FVector IncomingVelocity = FVector::ZeroVector;	// 월드 입사 속도

	float Time = 0.0f;	//검사 선분 위의 충돌 비율
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

// ---------------------------------------------------------------------------
// 렌더용 스냅샷 (UE ParticleHelper.h)
// 시뮬레이션이 끝난 뒤 인스턴스의 파티클 메모리를 복사해 렌더 쪽에 넘긴다.
// ---------------------------------------------------------------------------

// GPU에 보내는 스프라이트 파티클 하나 (인스턴싱, 파티클당 1개). 7단계 셰이더 입력과 순서·크기가 같아야 한다.
struct FParticleSpriteVertex
{
	FVector Position;
	float RelativeTime;
	FVector OldPosition;
	float ParticleId;
	FVector2 Size;
	float Rotation;
	float SubImageIndex;
	FLinearColor Color;
};
static_assert(sizeof(FParticleSpriteVertex) == 64, "FParticleSpriteVertex는 64바이트여야 한다.");
static_assert(offsetof(FParticleSpriteVertex, Position) == 0);
static_assert(offsetof(FParticleSpriteVertex, RelativeTime) == 12);
static_assert(offsetof(FParticleSpriteVertex, OldPosition) == 16);
static_assert(offsetof(FParticleSpriteVertex, ParticleId) == 28);
static_assert(offsetof(FParticleSpriteVertex, Size) == 32);
static_assert(offsetof(FParticleSpriteVertex, Rotation) == 40);
static_assert(offsetof(FParticleSpriteVertex, SubImageIndex) == 44);
static_assert(offsetof(FParticleSpriteVertex, Color) == 48);

enum EDynamicEmitterType : int32
{
	DET_Unknown,
	DET_Sprite,
	DET_Mesh,
	DET_Beam2,
	DET_Ribbon,
	DET_AnimTrail,
	DET_Custom,
};

// 파티클 데이터와 인덱스를 한 블록으로 담는 복사본. 인덱스는 블록 끝을 가리킨다.
struct FParticleDataContainer
{
	static constexpr uint32 Alignment = 16;
	static constexpr int32 MaxCapacity = 1 << 16;

	int32 MemBlockSize = 0;
	int32 ParticleDataNumBytes = 0;
	int32 ParticleIndicesNumShorts = 0;
	uint8* ParticleData = nullptr;     // 할당한 블록의 시작
	uint16* ParticleIndices = nullptr; // 따로 할당하지 않음. 블록 끝

	FParticleDataContainer() = default;
	~FParticleDataContainer() { Free(); }
	FParticleDataContainer(const FParticleDataContainer&) = delete;
	FParticleDataContainer& operator=(const FParticleDataContainer&) = delete;

	bool Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
	bool Allocate(int32 Capacity, int32 Stride);
	void Reset() { Free(); }
	void Free();
};

// 이미터 종류와 상관없는 스냅샷 데이터
struct FDynamicEmitterReplayDataBase
{
	EDynamicEmitterType eEmitterType = DET_Unknown;
	int32 ActiveParticleCount = 0;
	int32 ParticleStride = 0;
	FParticleDataContainer DataContainer;
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);
	int32 SortMode = 0;

	virtual ~FDynamicEmitterReplayDataBase() = default;
};

// 스프라이트 이미터의 스냅샷 데이터. Required 모듈의 렌더 설정을 복사해 둔다.
struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	UMaterial* MaterialInterface = nullptr;
	float InvDeltaSeconds = 0.0f;
	int32 MaxDrawCount = -1;          // -1이면 제한 없음
	int32 SubUVDataOffset = 0;
	int32 SubImages_Horizontal = 1;
	int32 SubImages_Vertical = 1;
	bool bUseLocalSpace = false;
	uint8 ScreenAlignment = 0;
	FVector2 PivotOffset = FVector2(-0.5f, -0.5f);
};

struct FDynamicSpriteEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
};

// 렌더 쪽이 다루는 이미터 하나의 데이터. 스냅샷(Source)과 그리는 방법을 함께 갖는다.
struct FDynamicEmitterDataBase
{
	explicit FDynamicEmitterDataBase(const UParticleModuleRequired* RequiredModule) {}
	virtual ~FDynamicEmitterDataBase() = default;

	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;

	bool bSelected = false;
	bool bValid = false;
	int32 EmitterIndex = 0;
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	explicit FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule)
		: FDynamicEmitterDataBase(RequiredModule)
	{
	}

	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const = 0;
	// 파티클 하나당 GPU로 보내는 바이트 수
	virtual int32 GetDynamicVertexStride() const = 0;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	explicit FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule)
		: FDynamicSpriteEmitterDataBase(RequiredModule)
	{
	}

	void Init(bool bInSelected)
	{
		bSelected = bInSelected;
		bValid = true;
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override { return Source; }
	virtual const FDynamicSpriteEmitterReplayDataBase* GetSourceData() const override { return &Source; }
	virtual int32 GetDynamicVertexStride() const override { return sizeof(FParticleSpriteVertex); }

	FDynamicSpriteEmitterReplayData Source;
};

// 컴포넌트 하나의 스냅샷: 이미터마다 하나씩. 소멸할 때 가진 데이터를 모두 지운다.
struct FParticleDynamicData
{
	TArray<FDynamicEmitterDataBase*> DynamicEmitterDataArray;

	FParticleDynamicData() = default;
	~FParticleDynamicData()
	{
		for (FDynamicEmitterDataBase* Data : DynamicEmitterDataArray)
		{
			delete Data;
		}
	}
	FParticleDynamicData(const FParticleDynamicData&) = delete;
	FParticleDynamicData& operator=(const FParticleDynamicData&) = delete;
};
