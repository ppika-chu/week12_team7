#pragma once

#include "ParticleModule.h"
#include "Render/Material.h"
#include "ParticleModuleRequired.generated.h"


//파티클이 아닌 이미터 자체의 설정을 담는 모듈
//값만 들고 있는 모듈
UCLASS()
class UParticleModuleRequired : public UParticleModule
{
	GENERATED_BODY()

public:

	//================================
	// 이미터 수명
	
	//이미터가 한 바퀴 도는 시간(초)
	UPROPERTY(EditAnywhere)
	float EmitterDuration = 1.f;

	//몇 바퀴 돌지. 0이면 무한 반복
	UPROPERTY(EditAnywhere)
	int32 EmitterLoops = 0;

	//시작 전 대기 시간(초)
	UPROPERTY(EditAnywhere)
	float EmitterDelay = 0.f;


	//================================
	// 그리기 설정

	UPROPERTY(EditAnywhere)
	UMaterial* Material = nullptr;

	//TODO : 빌보드 설정
	//UPROPERTY(EditAnywhere)
	//TEnumAsByte<EParticleScreenAlignment> ScreenAlignment;

	//TODO : 그리는 순서. 정렬 없음 / 카메라 거리순 / 나이순. 반투명 겹침을 바로잡을 때 필요
	/**
	 *	The sorting mode to use for this emitter.
	 *	PSORTMODE_None				- No sorting required.
	 *	PSORTMODE_ViewProjDepth		- Sort by view projected depth of the particle.
	 *	PSORTMODE_DistanceToView	- Sort by distance of particle to view in world space.
	 *	PSORTMODE_Age_OldestFirst	- Sort by age, oldest drawn first.
	 *	PSORTMODE_Age_NewestFirst	- Sort by age, newest drawn first.
	 */
	//UPROPERTY(EditAnywhere)
	//TEnumAsByte<EParticleSortMode> SortMode;

	//그리는 개수 상한
	UPROPERTY(EditAnywhere)
	bool bUseMaxDrawCount = false;

	UPROPERTY(EditAnywhere)
	int32 MaxDrawCount = 500;


	//================================
	// 좌표 설정

	// true면 파티클이 컴포넌트를 따라 움직임. false면 한 번 나온 파티클은 월드에 남음
	UPROPERTY(EditAnywhere)
	bool bUseLocalSpace = false;

	// 컴포넌트 기준 위치
	UPROPERTY(EditAnywhere)
	FVector EmitterOrigin;

	// 컴포넌트 기준 회전
	UPROPERTY(EditAnywhere)
	FRotator EmitterRotation;

	//================================
	// TODO : SubUV도 지원하나봐

	//SubImages_Horizontal, SubImages_Vertical	텍스처가 가로 세로 몇 칸짜리 시트인지
	//InterpolationMethod 칸을 넘기는 방식(없음 / 순차 / 블렌드 / 랜덤)
};