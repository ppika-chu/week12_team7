#include "EnginePCH.h"
#include "Particles/Color/ParticleModuleColor.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"

namespace
{
FLinearColor MakeParticleColor(const FVector& RGB, float Alpha, bool bClampAlpha)
{
	if (bClampAlpha)
	{
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	}
	return FLinearColor(RGB.X, RGB.Y, RGB.Z, Alpha);
}
}

// ---- Color (생성 시 색) ----

UParticleModuleColor::UParticleModuleColor()
{
	bSpawnModule = true;

	UDistributionVectorConstant* ColorDistribution = CreateDefaultSubobject<UDistributionVectorConstant>("DistributionStartColor");
	ColorDistribution->Constant = FVector(1.0f, 1.0f, 1.0f);
	StartColor.Distribution = ColorDistribution;

	UDistributionFloatConstant* AlphaDistribution = CreateDefaultSubobject<UDistributionFloatConstant>("DistributionStartAlpha");
	AlphaDistribution->Constant = 1.0f;
	StartAlpha.Distribution = AlphaDistribution;
}

void UParticleModuleColor::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	FParticleEmitterInstance& Owner = Context.Owner;
	FRandomStream& RandomStream = Owner.GetRandomStream();

	const FVector RGB = StartColor.GetValue(Owner.EmitterTime, &RandomStream);
	const float Alpha = StartAlpha.GetValue(Owner.EmitterTime, &RandomStream);
	Particle.Color = MakeParticleColor(RGB, Alpha, bClampAlpha);
	Particle.BaseColor = Particle.Color;
}

// ---- Color Over Life (수명에 따른 색) ----

UParticleModuleColorOverLife::UParticleModuleColorOverLife()
{
	bSpawnModule = true;
	bUpdateModule = true;

	// 기본값: 흰색 → 검정, 불투명 → 투명
	UDistributionVectorConstantCurve* ColorDistribution = CreateDefaultSubobject<UDistributionVectorConstantCurve>("DistributionColorOverLife");
	ColorDistribution->ConstantCurve.AddPoint(0.0f, FVector(1.0f, 1.0f, 1.0f));
	ColorDistribution->ConstantCurve.AddPoint(1.0f, FVector(0.0f, 0.0f, 0.0f));
	ColorOverLife.Distribution = ColorDistribution;

	UDistributionFloatConstantCurve* AlphaDistribution = CreateDefaultSubobject<UDistributionFloatConstantCurve>("DistributionAlphaOverLife");
	AlphaDistribution->ConstantCurve.AddPoint(0.0f, 1.0f);
	AlphaDistribution->ConstantCurve.AddPoint(1.0f, 0.0f);
	AlphaOverLife.Distribution = AlphaDistribution;
}

void UParticleModuleColorOverLife::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT

	// Spawn이 Update보다 뒤에 있어서, 막 생성된 파티클은 이번 프레임 Update를 거치지 않는다. 그래서 여기서 먼저 칠한다.
	const FVector RGB = ColorOverLife.GetValue(Particle.RelativeTime);
	const float Alpha = AlphaOverLife.GetValue(Particle.RelativeTime);
	Particle.Color = MakeParticleColor(RGB, Alpha, bClampAlpha);
	Particle.BaseColor = Particle.Color;
}

void UParticleModuleColorOverLife::Update(const FUpdateContext& Context)
{
	BEGIN_UPDATE_LOOP
		const FVector RGB = ColorOverLife.GetValue(Particle.RelativeTime);
		const float Alpha = AlphaOverLife.GetValue(Particle.RelativeTime);
		Particle.Color = MakeParticleColor(RGB, Alpha, bClampAlpha);
	END_UPDATE_LOOP
}
