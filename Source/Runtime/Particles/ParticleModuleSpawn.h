#pragma once

#include "Particles/ParticleModule.h"
#include "Distributions/DistributionFloat.h"

// 정해진 시각에 한꺼번에 생성할 개수
struct FParticleBurst
{
	int32 Count = 0;     // 생성 개수 (CountLow를 쓰면 최대값)
	int32 CountLow = -1; // -1이면 Count 고정, 아니면 [CountLow, Count] 랜덤
	float Time = 0.0f;   // 루프 시작 후 몇 초에 터질지
};

// 초당 생성 개수와 Burst를 정하는 모듈
// LOD 레벨의 Modules 목록에 들어가지 않고 SpawnModule 포인터로 따로 보관한다.
class UParticleModuleSpawn : public UParticleModule
{
	DECLARE_CLASS(UParticleModuleSpawn, UParticleModule)

public:
	UParticleModuleSpawn();
	virtual ~UParticleModuleSpawn() = default;

public:
	FRawDistributionFloat Rate;       // 초당 생성 개수
	FRawDistributionFloat RateScale;  // Rate에 곱하는 배율
	TArray<FParticleBurst> BurstList;
	FRawDistributionFloat BurstScale; // Burst 개수에 곱하는 배율
};
