#pragma once

#include "Components/SceneComponent.h"
#include "CascadeParticleSystemComponent.generated.h"

struct FBaseParticle
{
	//FVector Location;
	//FVector Velocity;
	//float RelativeTime;
	//float Lifetime;
	//FVector BaseVelocity;
	//float Rotation;
	//float RotationRate;
	//FVector Size;
	//FColor Color;
	//...
};

struct FParticleDataContainer
{
	//int32 MemBlockSize;
	//int32 ParticleDataNumBytes;
	//int32 ParticleIndicesNumShorts;
	//uint8* ParticleData;     // this is also the memory block we allocated
	//uint16* ParticleIndices; // not allocated, this is at the end of the memory block
	//...
};

struct FDynamicEmitterReplayDataBase
{
	///** The type of emitter. */
	//EDynamicEmitterType eEmitterType;

	///** The number of particles currently active in this emitter. */
	//int32 ActiveParticleCount;

	//int32 ParticleStride;
	//FParticleDataContainer DataContainer;

	//FVector3f Scale;

	//int32 SortMode;
	//...
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	//UMaterialInterface* MaterialInterface;
	//struct FParticleRequiredModule* RequiredModule;
	//...
};

struct FDynamicEmitterDataBase
{
	//int32 EmitterIndex;
	//...

	//	virtual const FDynamicEmitterReplayDataBase&
	//	GetSource() const = 0;
	//...
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	//void SortSpriteParticles(...);
	//virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const = 0;
	//...
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	//virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	//{
	//	return sizeof(FParticleSpriteVertex);
	//}
	//...
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	//virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const override
	//{
	//	return sizeof(FMeshParticleInstanceVertex);
	//}
	//...
};

struct FParticleEmitterInstance
{
	//UParticleEmitter* SpriteTemplate;

	//// Owner
	//UParticleSystemComponent* Component;

	//int32 CurrentLODLevelIndex;
	//UParticleLODLevel* CurrentLODLevel;

	///** Pointer to the particle data array. */
	//uint8* ParticleData;
	///** Pointer to the particle index array. */
	//uint16* ParticleIndices;
	///** Pointer to the instance data array. */
	//uint8* InstanceData;
	///** The size of the Instance data array. */
	//int32 InstancePayloadSize;
	///** The offset to the particle data. */
	//int32 PayloadOffset;
	///** The total size of a particle (in bytes). */
	//int32 ParticleSize;
	///** The stride between particles in the ParticleData array. */
	//int32 ParticleStride;
	///** The number of particles currently active in the emitter. */
	//int32 ActiveParticles;
	///** Monotonically increasing counter. */
	//uint32 ParticleCounter;
	///** The maximum number of active particles that can be held in
	// * the particle data array.
	// */
	//int32 MaxActiveParticles;
	///** The fraction of time left over from spawning. */

	//void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity, struct FParticleEventInstancePayload* EventPayload)
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

	//void KillParticle(int32 Index);

	//...
};


UCLASS()
class UParticleSystemComponent : public USceneComponent
{
	GENERATED_BODY()

	//TArray<struct FParticleEmitterInstance*> EmitterInstances;
	//UParticleSystem* Template;

	//TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	//...
};