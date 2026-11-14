#include "EnginePCH.h"
#include "ParticleHelper.h"
#include "Math/Noise.h"
#include "ParticleModuleLifetime.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	bSpawnModule = true;
}

void UParticleModuleLifetime::Spawn(const FSpawnContext& Context)
{
	SPAWN_INIT;
	{
		FParticleEmitterInstance* Owner = &Context.Owner;

		float MaxLifetime = FMath::FRandRange(LifetimeMin, LifetimeMax);

		if (Particle.OneOverMaxLifetime > 0.f)
		{
			// 다른 모듈이 이미 수명을 정했으면 기존 수명에 더한다.
			Particle.OneOverMaxLifetime = 1.f / (MaxLifetime + 1.f / Particle.OneOverMaxLifetime);
		}
		else
		{
			Particle.OneOverMaxLifetime = MaxLifetime > 0.f ? 1.f / MaxLifetime : 0.f;
		}

		// 프레임 중간에 태어난 만큼 나이를 미리 먹인다.
		Particle.RelativeTime = Particle.RelativeTime > 1.f
			? Particle.RelativeTime
			: Context.SpawnTime * Particle.OneOverMaxLifetime;
	}
}
