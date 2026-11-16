#include "EnginePCH.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"

#include "Particles/ParticleEmitterInstances.h"
#include "Particles/ParticleHelper.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	bSpawnModule = true;

	UDistributionFloatUniform* Distribution = CreateDefaultSubobject<UDistributionFloatUniform>("DistributionLifetime");
	Distribution->Min = 1.0f;
	Distribution->Max = 1.0f;
	Lifetime.Distribution = Distribution;
}

void UParticleModuleLifetime::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT
	FParticleEmitterInstance& Owner = Context.Owner;

	const float MaxLifetime = Lifetime.GetValue(Owner.EmitterTime, &Owner.GetRandomStream());
	if (Particle.OneOverMaxLifetime > 0.0f)
	{
		// 다른 Lifetime 모듈이 먼저 정했으면 수명을 더한다.
		Particle.OneOverMaxLifetime = 1.0f / (MaxLifetime + 1.0f / Particle.OneOverMaxLifetime);
	}
	else
	{
		Particle.OneOverMaxLifetime = MaxLifetime > 0.0f ? 1.0f / MaxLifetime : 0.0f;
	}

	// 생성 시점부터 프레임 끝까지 흐른 시간만큼 나이를 준다. 이미 1을 넘겼으면 다른 모듈이 죽이려는 것이라 그대로 둔다.
	if (Particle.RelativeTime <= 1.0f)
	{
		Particle.RelativeTime = Context.SpawnTime * Particle.OneOverMaxLifetime;
	}
}
