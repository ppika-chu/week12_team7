#pragma once
#include "EnginePCH.h"
#include "Math/Vector.h"
#include "Math/Color.h"

// 파티클 하나의 데이터
struct FBaseParticle
{
	 FVector Location;
	 FVector Velocity;

	 // 0(탄생) ~ 1(소멸)로 정규화된 나이
	 float RelativeTime;

	 // 수명(초). 구현에 따라 1/수명을 저장하기도 함
	 float Lifetime;

	 // 스폰 시 정해진 기준 속도
	 FVector BaseVelocity;

	 float Rotation;
	 float RotationRate;
	 FVector Size;
	 FColor Color;
	//...
};

struct FParticleDataContainer
{
	// int32 MemBlockSize;
	// int32 ParticleDataNumBytes;
	// int32 ParticleIndicesNumShorts;
	// uint8* ParticleData;     // this is also the memory block we allocated
	// uint16* ParticleIndices; // not allocated, this is at the end of the memory block
	//...
};

struct FDynamicEmitterReplayDataBase
{
	///** The type of emitter. */
	// EDynamicEmitterType eEmitterType;

	///** The number of particles currently active in this emitter. */
	// int32 ActiveParticleCount;

	// int32 ParticleStride;
	// FParticleDataContainer DataContainer;

	// FVector3f Scale;

	// int32 SortMode;
	//...
};

struct FDynamicSpriteEmitterReplayDataBase : public FDynamicEmitterReplayDataBase
{
	// UMaterialInterface* MaterialInterface;
	// struct FParticleRequiredModule* RequiredModule;
	//...
};

struct FDynamicEmitterDataBase
{
	// int32 EmitterIndex;
	//...

	//	virtual const FDynamicEmitterReplayDataBase&
	//	GetSource() const = 0;
	//...
};

struct FDynamicSpriteEmitterDataBase : public FDynamicEmitterDataBase
{
	// void SortSpriteParticles(...);
	// virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const = 0;
	//...
};

struct FDynamicSpriteEmitterData : public FDynamicSpriteEmitterDataBase
{
	// virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type InFeatureLevel) const override
	//{
	//	return sizeof(FParticleSpriteVertex);
	// }
	//...
};

struct FDynamicMeshEmitterData : public FDynamicSpriteEmitterData
{
	// virtual int32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const override
	//{
	//	return sizeof(FMeshParticleInstanceVertex);
	// }
	//...
};

//TODO : 매크로 구현
// DECLARE_PARTICLE_PTR
// BEGIN_UPDATE_LOOP
// END_UPDATE_LOOP