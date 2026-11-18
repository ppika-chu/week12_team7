#include "EnginePCH.h"
#include "Particles/Acceleration/ParticleModuleAcceleration.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"

UParticleModuleAcceleration::UParticleModuleAcceleration()
{
	bSpawnModule = true;
	bUpdateModule = true;
	Acceleration.SetDistribution(CreateDefaultSubobject<UDistributionVectorConstant>("DistributionAcceleration"));
}

uint32 UParticleModuleAcceleration::RequiredBytes(UParticleModuleTypeDataBase* TypeData)
{
	return sizeof(FAccelerationPayload);
}

bool UParticleModuleAcceleration::HasPayload(const FParticleEmitterInstance& Owner, int32 Offset)
{
	return Offset >= static_cast<int32>(sizeof(FBaseParticle)) &&
		Offset % alignof(FAccelerationPayload) == 0 &&
		Offset <= Owner.ParticleSize - static_cast<int32>(sizeof(FAccelerationPayload));
}

void UParticleModuleAcceleration::InitializePayload(FParticleEmitterInstance& Owner, FAccelerationPayload& Payload) const
{
	Payload.Value = Acceleration.GetValue(Owner.EmitterTime, &Owner.GetRandomStream());
	Payload.bInitialized = 1;
}

void UParticleModuleAcceleration::Spawn(const FSpawnContext& Context)
{
	if (!HasPayload(Context.Owner, Context.Offset))
	{
		return;
	}

	SPAWN_INIT
	PARTICLE_ELEMENT(FAccelerationPayload, Payload);
	InitializePayload(Context.Owner, Payload);

	// 생성 프레임의 남은 시간 반영
	const FVector VelocityDelta = Payload.Value * Context.SpawnTime;
	Particle.BaseVelocity += VelocityDelta;
	Particle.Velocity += VelocityDelta;
}

void UParticleModuleAcceleration::Update(const FUpdateContext& Context)
{
	if (!HasPayload(Context.Owner, Context.Offset))
	{
		return;
	}

	BEGIN_UPDATE_LOOP
	if ((Particle.Flags & STATE_Particle_FreezeTranslation) != 0)
	{
		CONTINUE_UPDATE_LOOP
	}

	PARTICLE_ELEMENT(FAccelerationPayload, Payload);
	if (Payload.bInitialized == 0)
	{
		// 생성 당시 비활성 모듈의 첫 샘플
		InitializePayload(Context.Owner, Payload);
	}

	const FVector VelocityDelta = Payload.Value * DeltaTime;
	Particle.BaseVelocity += VelocityDelta;
	Particle.Velocity += VelocityDelta;
	END_UPDATE_LOOP
}
