#pragma once

#include "UObject/Object.h"
#include "Containers/Array.h"
#include "ParticleLODLevel.generated.h"

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;

// Module을 Level에 따라 갈아 끼운다.
UCLASS()
class UParticleLODLevel : public UObject
{
	GENERATED_BODY()

public:
	UParticleLODLevel();

	//이미터 정보가 변경되었을 때(생성, 삭제) 호출
	void UpdateModuleLists();

	int32 Level;
	bool bEnabled;

	// 필수 모듈
	UParticleModuleRequired* RequiredModule;

	// 스폰 모듈
	UParticleModuleSpawn* SpawnModule;

	// 추가로 설정할 수 있는 모듈
	TArray<UParticleModule*> Modules;

	//Sprite, Mesh 구분 (없으면 Sprite)
	UParticleModuleTypeDataBase* TypeDataModule;

	//호출 시점별로 미리 분류해둔 캐시
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
};