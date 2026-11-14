#pragma once

#include "ParticleModule.h"
//#include "Containers/Array.h"
#include "ParticleEmitter.h"
#include "ParticleModuleSpawn.generated.h"

//스폰에 필요한 모듈, 로직이 없고 값만 담아두는 모듈이다.
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:

	//초 당 몇개를 낼지
	//TODO : FRawDistributionFloat
	UPROPERTY(EditAnywhere)
	float Rate = 20.f;

	//Rate에 곱하는 배율, 에디터에서 전체 양을 한 번에 줄일 때 사용
	UPROPERTY(EditAnywhere)
	float RateScale = 1;

	//t초에 한꺼번에 N개 목록(폭발)
	//TODO : 배열 리플렉션을 지원하면 추가
	//UPROPERTY(EditAnywhere)
	//TArray<FParticleBurst> BurstList;

	//버스트 배율
	UPROPERTY(EditAnywhere)
	float BurstScale = 1.f;

	//버스트 방식
	//UPROPERTY(EditAnywhere)
	//TODO : 어떻게 메소드 방식을 에디터에 표시하지
	EParticleBurstMethod ParticleBurstMethod;
};