#pragma once

#include "UObject/Object.h"
#include <Core/Types.h>
#include "ParticleEmitterInstances.h"
#include "ParticleHelper.h"
#include "ParticleModule.generated.h"

class UParticleModuleTypeDataBase;

UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:

	//SpawnModule 인지, UpdateModule인지
	bool bSpawnModule = false;
	bool bUpdateModule = false;

	/** If true, the module is enabled												*/
	UPROPERTY()
	bool bEnabled = true;

	/** If true, the module has had editing enabled on it							*/
	UPROPERTY()
	bool bEditable;

	struct FContext
	{
		FParticleEmitterInstance& Owner;
	};

	struct FSpawnContext : FContext
	{
		int32 Offset;
		float SpawnTime;
		FBaseParticle* ParticleBase;
	};
	struct FUpdateContext : FContext
	{
		int32 Offset;
		float DeltaTime;
	};

	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData);
	virtual void Spawn(const FSpawnContext &Context) {}
	virtual void Update(const FUpdateContext &Context) {}
};

/*
UParticleModuleRequired: Emitter에 필수적인 설정을 포함하며, 파티클의 기본 속성들을 정의합니다.
UParticleModuleSpawn: 파티클의 생성 빈도와 수량을 제어합니다.
UParticleModuleLifetime: 파티클의 수명을 설정합니다.
UParticleModuleLocation: 파티클의 초기 위치를 결정합니다.
UParticleModuleVelocity: 파티클의 초기 속도와 방향을 설정합니다.
UParticleModuleColor: 파티클의 색상을 정의하며, 시간에 따른 색상 변화를 설정할 수 있습니다.
UParticleModuleSize: 파티클의 크기를 설정합니다.
*/