#pragma once

#include "Containers/Array.h"
#include "Asset/RenderAsset.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UFXSystemAsset : public URenderAsset
{
	GENERATED_BODY()

	//TODO : Pool, PSO
};



UCLASS()
class UParticleSystem : public UFXSystemAsset
{
	GENERATED_BODY()

public:
	// 코드로 조립한 기본 파티클 시스템. 처음 요청될 때 만들어 에셋 매니저에 등록한다.
	static UParticleSystem* GetDefault();

	// 파일 저장과 불러오기는 FParticleSystemLoader가 맡는다.

	// 에셋 구조가 정해지거나 바뀐 뒤 반드시 호출
	void UpdateAndBuild();

	//파티클 한 개의 크기 계산
	void BuildEmitters();


	TArray<UParticleEmitter*> Emitters;
};
