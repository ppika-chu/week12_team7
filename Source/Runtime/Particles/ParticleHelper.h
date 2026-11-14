#pragma once
#include "EnginePCH.h"
#include "Math/Vector.h"
#include "Math/Color.h"

class UMaterialInterface;

// 파티클 하나의 데이터
struct FBaseParticle
{
	// 24 bytes
	FVector OldLocation; // Last frame's location, used for collision
	FVector Location;    // Current location

	// 16 bytes
	FVector BaseVelocity; // Velocity = BaseVelocity at the start of each frame.
	float Rotation;         // Rotation of particle (in Radians)

	// 16 bytes
	FVector Velocity;     // Current velocity, gets reset to BaseVelocity each frame to allow
	float BaseRotationRate; // Initial angular velocity of particle (in Radians per second)

	// 16 bytes
	FVector BaseSize; // Size = BaseSize at the start of each frame
	float RotationRate; // Current rotation rate, gets reset to BaseRotationRate each frame

	// 16 bytes
	FVector Size; // Current size, gets reset to BaseSize each frame
	int32 Flags;    // Flags indicating various particle states

	// 16 bytes
	FLinearColor Color; // Current color of particle.

	// 16 bytes
	FLinearColor BaseColor; // Base color of the particle

	// 16 bytes
	float RelativeTime;       // Relative time, range is 0 (==spawn) to 1 (==death)
	float OneOverMaxLifetime; // Reciprocal of lifetime
	float Placeholder0;
	float Placeholder1;
};

// 일반적인 인스턴스 PayLoad
struct FParticleEventInstancePayload
{
	uint32 bSpawnEventsPresent : 1;
	uint32 bDeathEventsPresent : 1;
	uint32 bCollisionEventsPresent : 1;
	uint32 bBurstEventsPresent : 1;

	int32 SpawnTrackingCount;
	int32 DeathTrackingCount;
	int32 CollisionTrackingCount;
	int32 BurstTrackingCount;
};

//파티클 상태 플래그
enum EParticleStates
{
	/** Ignore updates to the particle						*/
	STATE_Particle_JustSpawned = 0x02000000,
	/** Ignore updates to the particle						*/
	STATE_Particle_Freeze = 0x04000000,
	/** Ignore collision updates to the particle			*/
	STATE_Particle_IgnoreCollisions = 0x08000000,
	/**	Stop translations of the particle					*/
	STATE_Particle_FreezeTranslation = 0x10000000,
	/**	Stop rotations of the particle						*/
	STATE_Particle_FreezeRotation = 0x20000000,
	/** Combination for a single check of 'ignore' flags	*/
	STATE_Particle_CollisionIgnoreCheck = STATE_Particle_Freeze | STATE_Particle_IgnoreCollisions | STATE_Particle_FreezeTranslation | STATE_Particle_FreezeRotation,
	/** Delay collision updates to the particle				*/
	STATE_Particle_DelayCollisions = 0x40000000,
	/** Flag indicating the particle has had at least one collision	*/
	STATE_Particle_CollisionHasOccurred = 0x80000000,
	/** State mask. */
	STATE_Mask = 0xFE000000,
	/** Counter mask. */
	STATE_CounterMask = (~STATE_Mask)
};

/*-----------------------------------------------------------------------------
	Particle Dynamic Data
-----------------------------------------------------------------------------*/

/**
 * Dynamic particle emitter types
 *
 * NOTE: These are serialized out for particle replay data, so be sure to update all appropriate
 *    when changing anything here.
 */
enum EDynamicEmitterType
{
	DET_Unknown = 0,
	DET_Sprite,
	DET_Mesh,
	DET_Beam2,
	DET_Ribbon,
	DET_AnimTrail,
	DET_Custom
};


//복사본 메모리 한 덩어리
struct FParticleDataContainer
{
	 int32 MemBlockSize;
	 int32 ParticleDataNumBytes;
	 int32 ParticleIndicesNumShorts;
	 uint8* ParticleData;     // this is also the memory block we allocated
	 uint16* ParticleIndices; // not allocated, this is at the end of the memory block

	 FParticleDataContainer()
		 : MemBlockSize(0)
		 , ParticleDataNumBytes(0)
		 , ParticleIndicesNumShorts(0)
		 , ParticleData(nullptr)
		 , ParticleIndices(nullptr)
	 {
	 }
	 ~FParticleDataContainer()
	 {
		 Free();
	 }

	 //파티클과 index를 한 블록에 잡는다.
	 void Alloc(int32 InParticleDataNumBytes, int32 InParticleIndicesNumShorts);
	 void Free();
};

//복사한 데이터(Source)
struct FDynamicEmitterReplayDataBase
{
	/** The type of emitter. */
	 EDynamicEmitterType eEmitterType = DET_Unknown;

	/** The number of particles currently active in this emitter. */
	 int32 ActiveParticleCount = 0;
	 int32 ParticleStride = 0;
	 FParticleDataContainer DataContainer;
	 FVector Scale = FVector(1.f, 1.f, 1.f);
	 int32 SortMode = 0; // PSORTMODE_None
	//...

	 virtual ~FDynamicEmitterReplayDataBase() = default;
};

//스프라이트 계열(스프라이트, 메시) 공통 설정
struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	 UMaterialInterface* MaterialInterface = nullptr;
	 struct FParticleRequiredModule* RequiredModule = nullptr;
	 int32 MaxDrawCount = -1; // -1이면 제한 없음
	 bool bUseLocalSpace = false;
	//...
};

//스프라이트 이미터의 Source. 스프라이트 전용 데이터가 생기면 여기에 둔다.
struct FDynamicSpriteEmitterReplayData : public FDynamicSpriteEmitterReplayDataBase
{
};

//렌더 쪽이 다루는 단위
struct FDynamicEmitterDataBase
{
	FDynamicEmitterDataBase(const class UParticleModuleRequired* RequiredModule);
	virtual ~FDynamicEmitterDataBase() = default;

	bool bSelected;
	bool bValid;
	int32 EmitterIndex;
	
	virtual const FDynamicEmitterReplayDataBase& GetSource() const = 0;
};

//FDynamicSpriteEmitterReplayDataBase + FDynamicEmitterDataBase
struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{

	FDynamicSpriteEmitterDataBase(const UParticleModuleRequired* RequiredModule)
		: FDynamicEmitterDataBase(RequiredModule)
		, MaterialResource(nullptr)
	{
	}

	// void SortSpriteParticles(...);
	// virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const = 0;
	//...

	const struct FMaterialRenderProxy* MaterialResource;
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	FDynamicSpriteEmitterData(const UParticleModuleRequired* RequiredModule)
		: FDynamicSpriteEmitterDataBase(RequiredModule)
	{
	}

	virtual const FDynamicEmitterReplayDataBase& GetSource() const override
	{
		return Source;
	}

	virtual void Init(bool bInSelected);

	// virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	//{
	//	return sizeof(FParticleSpriteVertex);
	// }
	//...

	FDynamicSpriteEmitterReplayData Source;
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	// virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const override
	//{
	//	return sizeof(FMeshParticleInstanceVertex);
	// }
	//...
};

#define DECLARE_PARTICLE(Name, Address) \
	FBaseParticle& Name = *((FBaseParticle*)(Address)); // 참조로 받음

#define DECLARE_PARTICLE_PTR(Name, Address) \
	FBaseParticle* Name = (FBaseParticle*)(Address); // 포인터로 받음

#define BEGIN_UPDATE_LOOP															\
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

#define END_UPDATE_LOOP																\
			CurrentOffset = Offset;													\
		}																			\
	}

#define SPAWN_INIT	\
	const int32 ActiveParticles = Context.Owner.ActiveParticles;	\
	const int32 ParticleStride = Context.Owner.ParticleStride;		\
	int32 CurrentOffset = Context.Offset;							\
	FBaseParticle* ParticleBase = Context.ParticleBase;				\
	FBaseParticle& Particle = *(ParticleBase);