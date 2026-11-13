#pragma once
#include <Core/Types.h>

class UParticleEmitter;
class UParticleSystemComponent;
class UParticleLODLevel;

/*-----------------------------------------------------------------------------
	Information compiled from modules to build runtime emitter data.
-----------------------------------------------------------------------------*/
struct FParticleEmitterBuildInfo
{
};

// 이미터 하나의 실행 상태, 파티클 메모리 보유
struct FParticleEmitterInstance
{
	UParticleEmitter* SpriteTemplate;

	//// Owner
	UParticleSystemComponent* Component;

	int32 CurrentLODLevelIndex;
	UParticleLODLevel* CurrentLODLevel;

	// 대량의 파티클을 다루기 위해 직접 할당한 바이트 블록을 사용한다.
	///** Pointer to the particle data array. */
	uint8* ParticleData;
	///** Pointer to the particle index array. */
	uint16* ParticleIndices;
	///** Pointer to the instance data array. */
	uint8* InstanceData;
	///** The size of the Instance data array. */
	int32 InstancePayloadSize;
	///** The offset to the particle data. */
	 int32 PayloadOffset;
	///** The total size of a particle (in bytes). */
	int32 ParticleSize;
	///** The stride between particles in the ParticleData array. */
	int32 ParticleStride;
	///** The number of particles currently active in the emitter. */
	int32 ActiveParticles;
	///** Monotonically increasing counter. */
	uint32 ParticleCounter;
	///** The maximum number of active particles that can be held in  the particle data array.
	int32 MaxActiveParticles;
	///** The fraction of time left over from spawning. */

	FParticleEmitterInstance(UParticleSystemComponent& InComponent);
	~FParticleEmitterInstance();

	void InitParameters(UParticleEmitter* Emitter);
	void Init();
	bool Resize(int32 NewMax);

	void Tick(float DeltaTime)
	{
		// 수명 계산 후
	}
	void KillParticle(int32 Index)
	{
	}
	// void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload)
	//{
	//	for (int32 i = 0; i < Count; i++)
	//	{
	//		DECLARE_PARTICLE_PTR
	//		PreSpawn(Particle, InitialLocation, InitialVelocity);

	//		for (int32 ModuleIndex = 0; ModuleIndex < LODLevel->SpawnModules.Num(); ModuleIndex++)
	//		{
	//			...
	//		}
	//		PostSpawn(Particle, Interp, SpawnTime);
	//	}
	//}

	//...
};