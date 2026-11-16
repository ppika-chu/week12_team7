#include "EnginePCH.h"
#include "Particles/Size/ParticleModuleSize.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"

// ---- Size (생성 시 크기) ----

UParticleModuleSize::UParticleModuleSize()
{
	bSpawnModule = true;

	UDistributionVectorUniform* Distribution = CreateDefaultSubobject<UDistributionVectorUniform>("DistributionStartSize");
	Distribution->Min = FVector(1.0f, 1.0f, 1.0f);
	Distribution->Max = FVector(1.0f, 1.0f, 1.0f);
	StartSize.Distribution = Distribution;
}

void UParticleModuleSize::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	FParticleEmitterInstance& Owner = Context.Owner;

	const FVector Size = StartSize.GetValue(Owner.EmitterTime, &Owner.GetRandomStream());
	Particle.Size += Size;
	Particle.BaseSize += Size;
}

// ---- Size By Life (수명에 따른 배율) ----

UParticleModuleSizeMultiplyLife::UParticleModuleSizeMultiplyLife()
{
	bSpawnModule = true;
	bUpdateModule = true;

	// 기본값: 배율 1 유지. 키가 없으면 0이 되어 크기가 사라지므로 두 키를 넣어 둔다.
	UDistributionVectorConstantCurve* Distribution = CreateDefaultSubobject<UDistributionVectorConstantCurve>("DistributionLifeMultiplier");
	Distribution->ConstantCurve.AddPoint(0.0f, FVector(1.0f, 1.0f, 1.0f));
	Distribution->ConstantCurve.AddPoint(1.0f, FVector(1.0f, 1.0f, 1.0f));
	LifeMultiplier.Distribution = Distribution;
}

void UParticleModuleSizeMultiplyLife::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	const FVector Scale = LifeMultiplier.GetValue(Particle.RelativeTime);
	if (MultiplyX) { Particle.Size.X *= Scale.X; }
	if (MultiplyY) { Particle.Size.Y *= Scale.Y; }
	if (MultiplyZ) { Particle.Size.Z *= Scale.Z; }
}

void UParticleModuleSizeMultiplyLife::Update(const FUpdateContext& Context)
{
	// Size는 매 프레임 Reset에서 BaseSize로 돌아오므로 곱해도 누적되지 않는다.
	BEGIN_UPDATE_LOOP
		const FVector Scale = LifeMultiplier.GetValue(Particle.RelativeTime);
		if (MultiplyX) { Particle.Size.X *= Scale.X; }
		if (MultiplyY) { Particle.Size.Y *= Scale.Y; }
		if (MultiplyZ) { Particle.Size.Z *= Scale.Z; }
	END_UPDATE_LOOP
}

// ---- Size Scale By Speed (속도에 따른 배율) ----

UParticleModuleSizeScaleBySpeed::UParticleModuleSizeScaleBySpeed()
{
	bUpdateModule = true;
}

void UParticleModuleSizeScaleBySpeed::Update(const FUpdateContext& Context)
{
	const FVector Scale(SpeedScale.X, SpeedScale.Y, 1.0f);
	const FVector ScaleMax(MaxScale.X, MaxScale.Y, 1.0f);

	BEGIN_UPDATE_LOOP
		FVector Size = Scale * Particle.Velocity.Size();
		Size = FVector::Max(Size, FVector(1.0f));
		Size = FVector::Min(Size, ScaleMax);
		Particle.Size = Particle.BaseSize * Size;
	END_UPDATE_LOOP
}
